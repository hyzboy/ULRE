# ShaderGen 与渲染器彻底分离重构计划（可行性 + 执行方案）

**版本**：v1.5  
**日期**：2026-03-03  
**状态**：执行中（Phase 3 主路径切换进行中；已明确 Phase 6 目标为 ShaderGen 独立程序化）

---

## 0. 当前进展（2026-03-03）

已完成：

- 新增契约头文件 [inc/hgl/shadergen/contract/ShaderGenContract.h](inc/hgl/shadergen/contract/ShaderGenContract.h)
  - 提供 `ShaderGenRequest` / `ShaderGenResult` 及配套 DTO（layout、vertex input、buffer struct、diagnostics、cache key）
  - 该头文件不依赖渲染器实现对象（无 `VulkanDevAttr` / `MaterialManager` 等）
- 新增镜像构建器并接入编译路径
  - [inc/hgl/shadergen/contract/ShaderGenResultBuilder.h](inc/hgl/shadergen/contract/ShaderGenResultBuilder.h)
  - [src/ShaderGen/contract/ShaderGenResultBuilder.cpp](src/ShaderGen/contract/ShaderGenResultBuilder.cpp)
  - [src/ShaderGen/MaterialCompiler.cpp](src/ShaderGen/MaterialCompiler.cpp) 在 `CompileFixedMaterial` 成功后并行导出 `ShaderGenResult`，执行 descriptor/stage/vertex-input 数量一致性诊断（non-blocking）
- Phase 1 镜像字段已扩展
  - `ShaderGenResultBuilder` 已导出 `vertex_layout.attributes`（来自 VS 输入）
  - `ShaderGenResultBuilder` 已导出 `spv_per_stage`（来自 `shader_map` + `GetSPVData/GetSPVSize`）
  - mirror diagnostics warning 已接入统一日志输出
- Phase 2 起步：Renderer adapter 骨架已落地（只读消费）
  - 新增 [inc/hgl/graph/module/RendererShaderGenAdapter.h](inc/hgl/graph/module/RendererShaderGenAdapter.h)
  - 新增 [src/SceneGraph/module/RendererShaderGenAdapter.cpp](src/SceneGraph/module/RendererShaderGenAdapter.cpp)
  - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 已接入 non-blocking 只读消费调用，旧建材路径保持不变
- Phase 2 补充：双轨 diff 文本化输出已接入
  - `RendererShaderGenAdapter` 现输出 legacy vs mirror 的 `layout/vertex/spv` 对照摘要：`count/hash/match`
  - mismatch 不阻断旧路径，仅用于诊断与验收证据收集
  - diff 日志已升级为统一 `key=value` 结构，并附带 `spv stage` 汇总字段，便于按材质/阶段 grep 聚合
  - 已将聚合统计升级为 `RendererShaderGenAdapter` 内置 Profiler：运行期累计 `match/count/stage-combo`，当前阶段仅采集不输出（为后续可视化/日志面板预留）
  - 已提供最小调试入口：可通过 `GraphicsContext` / `MaterialManager` 读取或重置 ShaderGen Profiler 快照（无默认输出）
  - 适配器校验链路已结构化：新增 `ValidationReport`（error/warning + diff/result/request-result 状态），`MaterialManager` 改为消费状态对象而非仅依赖 stderr 文本
  - 新增 Validation 历史查询：支持 `GetLastValidationReport`、`GetRecentValidationReports`、`GetRecentValidationReportsByMaterial`（最近 N 条、按材质索引），默认仅内存采集不输出
  - result 结构校验已下沉到 contract 层：`ValidateShaderGenResult`（version/SPV/binding uniqueness/diagnostics），`RendererShaderGenAdapter` 仅做状态聚合与编排
  - legacy/mirror diff 计算已抽离到 contract-side 工具：`ShaderGenMirrorDiff`（`BuildShaderGenMirrorDiffSummary`），adapter 侧改为消费摘要结果
- Phase 2 并行入口已增加
  - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 新增 contract-aware 私有入口：可显式接收预构建 `ShaderGenResult`
  - 现有 `CreateMaterial` 已改为“先尝试预构建 mirror result，再转发到并行入口”，为后续主路径切换预留开关点
  - 新增 `ShaderGenRequestBuilder`（`MaterialCreateInfo -> ShaderGenRequest`）并接入 `MaterialManager`，形成 Request/Result 双 DTO 并行链路
  - `RendererShaderGenAdapter` 新增 `ConsumeRequestResultReadOnly(...)`：对 `required_resources` 与 `vertex_requirements` 做 request/result 一致性校验（non-blocking）
  - request/result 一致性校验逻辑已下沉到 ShaderGen contract 层：新增 `ShaderGenContractValidator`（`ValidateShaderGenRequestResult`），渲染器适配层改为调用该 contract API
  - 新增运行时路径模式（应用层显式配置）：
    - `legacy-only`：禁用 mirror 校验，仅走旧路径
    - `mirror-validate`（默认）：mirror 诊断并行执行，失败不阻断
    - `mirror-preferred`：mirror 预构建/校验失败时中止材质创建（严格模式）
  - 日志细节已按模式分流：
    - `mirror-validate` 输出 `DiffKV summary`
    - `mirror-preferred` 输出完整 `DiffKV`（layout/vertex/spv/summary）
- Phase 2 回归补齐
  - 新增 [inc/hgl/graph/module/ShaderGenPathMode.h](inc/hgl/graph/module/ShaderGenPathMode.h) 统一模式解析/命名函数
  - 新增 [test/ShaderGenPathModeTest.cpp](test/ShaderGenPathModeTest.cpp) 覆盖模式解析与日志级别映射回归（已通过）
  - 新增 `ShaderGenPathPolicy`（mode -> validate/strict/full-diff）统一策略对象，`MaterialManager` 已改为消费该策略
  - `GraphicsContext` 改为显式注入 `ShaderGenPathMode` 并构建 policy，`MaterialManager` 不再直接读取环境变量
  - [src/Work/AppFramework.cpp](src/Work/AppFramework.cpp) 已在创建 `GraphicsContext` 时显式传入模式；[inc/hgl/framework/AppFramework.h](inc/hgl/framework/AppFramework.h) 提供 `SetShaderGenPathMode(...)`
  - `AppFramework` 现为纯显式配置路径：默认 `mirror-validate`，可通过 `SetShaderGenPathMode(...)` / `SetShaderGenPathModeName(...)` 注入；已移除环境变量回退读取
  - `AppFramework` 新增 `Init(w,h,argc,argv)` 重载，支持 `--shadergen-path-mode=<mode>` / `--shadergen-path-mode <mode>` 命令行注入
  - [inc/hgl/framework/WorkManager.h](inc/hgl/framework/WorkManager.h) 新增 `RunFramework(title,argc,argv,...)` 重载（`os_char` 参数解析），可在示例入口直接透传命令行模式
  - [example/Basic/draw_triangle.cpp](example/Basic/draw_triangle.cpp) 已接入新重载，形成端到端入口样例
    - 已扩展多组示例入口透传 `argc/argv`（Texture/GUI/Geometry/Gizmo/Environment）
    - 已完成一轮 22 个剩余示例入口批量迁移（`os_main` 参数透传到 `RunFramework`）
- 相关 ShaderGen/测试链路已完成一轮稳定化回归（近邻测试通过），可作为后续 Phase 1 的安全基线
- 2026-03-03 收敛更新（Adapter 职责与 API 面精简）
  - `MaterialManager` 不再手工拼接 `ValidationReport`，改为统一调用 `ValidateMaterialContractReadOnly(...)`
  - 旧 bool 风格消费接口已删除：`ConsumeMaterialReadOnly/ConsumePairReadOnly/ConsumeRequestResultReadOnly/ConsumeResultReadOnly`
  - `ValidateResultReadOnly/ValidatePairReadOnly/ValidateRequestResultReadOnly` 已收敛为 adapter 内部私有分层接口
  - `RendererShaderGenAdapter` 内部存储已拆分为 profiler storage 与 validation-report storage（独立 mutex）
  - `ValidateMaterialContractReadOnly` 与 contract-check 到 report 的字段映射已完成去重（无行为变更）
  - 全量 CMake 构建已连续通过，关键链路无新增编译错误
- 2026-03-03 Phase 3 推进（主路径切换首段）
  - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 已将 shader module 构建逻辑切为“**优先使用 `ShaderGenResult.spv_per_stage`**”
  - 在 `mirror-validate` 下，若 mirror SPV 构建失败将自动回退 legacy 路径（non-blocking）
  - 在 `mirror-preferred` 下，mirror SPV 构建失败保持严格失败并中止材质创建（blocking）
  - 顶点布局/descriptor 严格一致性校验仅在“实际使用 mirror SPV 构建成功”后执行，避免 fallback 路径误报
  - `descriptor set_type` 差异已降级为非阻断（warning 语义），严格门禁仅对 Vulkan 关键字段生效：`set/binding/descriptor_type/stage/name`
- 2026-03-04 Phase 3 回归补强（严格失败链路）
  - [inc/hgl/graph/module/RendererShaderGenAdapter.h](inc/hgl/graph/module/RendererShaderGenAdapter.h) / [src/SceneGraph/module/RendererShaderGenAdapter.cpp](src/SceneGraph/module/RendererShaderGenAdapter.cpp) 新增 `ResetValidationReports()`，用于清空 validation 历史并保证回归测试可重复。
  - 新增 `GetRecentValidationMaterialCategoryMatrix(max_count)` 聚合接口，输出最近 validation 的“材质 × 分类”计数矩阵，便于按材质维度查看 `StrictGate.*` 分布。
  - 新增 [test/RendererShaderGenAdapterStrictGateTest.cpp](test/RendererShaderGenAdapterStrictGateTest.cpp)，覆盖 `StrictGate.Prebuild/Vertex/Descriptor` 三类错误的记录、最近历史顺序、按材质分组与分类直方图。
  - 同一测试已扩展覆盖“材质 × 分类”聚合矩阵正确性（`StrictMatA/StrictMatB`）。
  - [inc/hgl/ecs/core/Context.h](inc/hgl/ecs/core/Context.h) / [src/ecs/core/ContextDebug.cpp](src/ecs/core/ContextDebug.cpp) 新增 ECS Debug 桥接查询：
    - `GetShaderGenValidationCategoryHistogram(...)`
    - `GetShaderGenValidationMaterialCategoryMatrix(...)`
  - [src/ecs/core/Context.cpp](src/ecs/core/Context.cpp) 已在 `descriptor_contract_diag_log_enabled` 周期日志中接入 strict gate 摘要输出（`strict_total/prebuild/spv/vertex/descriptor/strict_materials`）。
  - [test/CMakeLists.txt](test/CMakeLists.txt) 已接入 `test_RendererShaderGenAdapterStrictGate`，本地 Debug 构建与执行通过。
- 2026-03-04 Phase 3 主路径推进（descriptor/pipeline layout）
  - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 的 descriptor 构建已调整为“**mirror result 优先**”：只要 `ShaderGenResult` 可用且与当前 SPV 路径兼容，即优先使用 contract 结果驱动 `MaterialDescriptorManager`/pipeline layout。
  - 对 legacy SPV 回退路径新增安全策略：当 mirror descriptor 校验或构建失败时，`mirror-validate` 自动回退 legacy descriptor layout，`mirror-preferred` 保持严格失败（`StrictGate.Descriptor`）。
  - `MaterialManager` 的 vertex input 构建已同步调整为“**mirror result 优先**”：新增 `BuildVertexInputFromMirrorResult(...)`，在 mirror 可用且兼容时优先使用 contract 顶点布局；`mirror-validate` 下失败自动回退 legacy vertex input，`mirror-preferred` / mirror SPV 主路径保持严格失败（`StrictGate.Vertex`）。
  - 已将“contract 顶点布局 -> 渲染顶点输入”转换逻辑从 `MaterialManager` 抽离为独立适配器：
    - [inc/hgl/graph/module/ShaderGenVertexInputAdapter.h](inc/hgl/graph/module/ShaderGenVertexInputAdapter.h)
    - [src/SceneGraph/module/ShaderGenVertexInputAdapter.cpp](src/SceneGraph/module/ShaderGenVertexInputAdapter.cpp)
  - 新增回归测试 [test/ShaderGenVertexInputAdapterTest.cpp](test/ShaderGenVertexInputAdapterTest.cpp)，覆盖：
    - `TransformID/MaterialInstanceID` 分组映射正确性；
    - contract 顶点布局转换成功路径；
    - duplicated location 失败路径。
  - `auto_instance` 的 `not found VAB "TransformID"` 报错已回归验证为 0 命中（`FATAL_MATCH_COUNT=0`）。
  - 对 [example/Environment/BasicLitSunDirectionECS.cpp](example/Environment/BasicLitSunDirectionECS.cpp) 完成双模式运行回归：
    - `mirror-validate`：关键 pattern（`read-only validation failed` / `mirror-preferred build aborted` / descriptor 更新错误）0 命中，进程退出码 `-1073741819`。
    - `mirror-preferred`：关键 pattern 同样 0 命中，进程退出码 `-1073741819`。
    - 两模式日志尾部均停在 RenderPass 创建后，当前可判定“该崩溃暂未体现为 ShaderGen contract 校验/descriptor 门禁错误”。
  - 本地定向构建 `ULRE.SceneGraph` 通过；`05_Billboard --shadergen-path-mode=mirror-validate` 运行仍崩溃（`exit code=-1073741819`），与“运行态退出根因待定位”阻塞项一致，未新增 descriptor 关键错误 grep 命中。
  - 已将 `MaterialManager` 内 descriptor contract helper 抽离为独立适配器：
    - [inc/hgl/graph/module/ShaderGenDescriptorLayoutAdapter.h](inc/hgl/graph/module/ShaderGenDescriptorLayoutAdapter.h)
    - [src/SceneGraph/module/ShaderGenDescriptorLayoutAdapter.cpp](src/SceneGraph/module/ShaderGenDescriptorLayoutAdapter.cpp)
    - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 已改为调用 `ValidateContractDescriptorLayoutAgainstLegacy(...)` / `BuildShaderDescriptorsFromContractLayout(...)`。
  - 新增回归测试 [test/ShaderGenDescriptorLayoutAdapterTest.cpp](test/ShaderGenDescriptorLayoutAdapterTest.cpp)，覆盖：
    - descriptor 关键字段对齐校验通过路径（`set/binding/type/stage/name`）；
    - descriptor type 不匹配失败路径；
    - `set_type` 继承 legacy 与按 set index fallback 规则。
  - [test/CMakeLists.txt](test/CMakeLists.txt) 已接入 `test_ShaderGenDescriptorLayoutAdapter`，本地 Debug 构建与执行通过。
  - 最新定向回归：`02_auto_instance` 与 `03_BasicLitSunDirectionECS` 重新构建并短时运行，关键 pattern（`not found VAB "TransformID"` / `FATAL ERROR` / `Validation Error` / `vkCmdCopyBufferToImage`）均为 0 命中。
  - 进一步职责下沉：`MaterialManager` 内部的 mirror vertex 严格一致性校验逻辑已迁移至 [src/SceneGraph/module/ShaderGenVertexInputAdapter.cpp](src/SceneGraph/module/ShaderGenVertexInputAdapter.cpp) 的 `ValidateContractVertexLayoutAgainstLegacy(...)`，`MaterialManager` 仅保留流程编排与门禁策略。
  - 继续职责下沉：`MaterialManager` 内联的 mirror SPV 模块构建流程已迁移至独立适配器：
    - [inc/hgl/graph/module/ShaderGenSPVModuleAdapter.h](inc/hgl/graph/module/ShaderGenSPVModuleAdapter.h)
    - [src/SceneGraph/module/ShaderGenSPVModuleAdapter.cpp](src/SceneGraph/module/ShaderGenSPVModuleAdapter.cpp)
    - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 改为调用 `BuildShaderModulesFromContractSPV(...)`。
  - 同一适配器已扩展承接 legacy fallback SPV 组装：新增 `BuildShaderModulesFromLegacySCIMap(...)`，`MaterialManager` 的 legacy 循环创建逻辑已移除，进一步收敛为流程编排。
  - 顶点输入策略分支已抽离为独立适配器：
    - [inc/hgl/graph/module/ShaderGenVertexPolicyAdapter.h](inc/hgl/graph/module/ShaderGenVertexPolicyAdapter.h)
    - [src/SceneGraph/module/ShaderGenVertexPolicyAdapter.cpp](src/SceneGraph/module/ShaderGenVertexPolicyAdapter.cpp)
    - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 改为通过 `BuildVertexInputByContractPolicy(...)` 统一处理 `UseMirror/UseLegacy/StrictAbort` 决策，减少内联分支复杂度。
  - descriptor 策略分支已抽离为独立适配器：
    - [inc/hgl/graph/module/ShaderGenDescriptorPolicyAdapter.h](inc/hgl/graph/module/ShaderGenDescriptorPolicyAdapter.h)
    - [src/SceneGraph/module/ShaderGenDescriptorPolicyAdapter.cpp](src/SceneGraph/module/ShaderGenDescriptorPolicyAdapter.cpp)
    - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 改为通过 `BuildDescriptorsByContractPolicy(...)` 统一处理 `UseMirror/UseLegacy/StrictAbort`，并保留 layout/build 两类 fallback 语义。
  - strict gate 上报与日志已去重抽离：
    - [inc/hgl/graph/module/ShaderGenContractGateReporter.h](inc/hgl/graph/module/ShaderGenContractGateReporter.h)
    - [src/SceneGraph/module/ShaderGenContractGateReporter.cpp](src/SceneGraph/module/ShaderGenContractGateReporter.cpp)
    - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 改为统一调用 `ReportMirrorPreferredStrictAbort(...)`，移除重复的 `RecordExternalValidationError + fprintf` 片段。
  - fallback 日志文本拼接已进一步下沉到 reporter：新增 `ReportMirrorSPVFallback(...)` / `ReportMirrorVertexFallback(...)` / `ReportMirrorDescriptorFallback(...)`，`MaterialManager` 不再内联 fallback 日志格式化。
  - `CreateMaterial` 前置路径判定已抽离为 `ContractPathContext`：
    - [inc/hgl/graph/module/ShaderGenContractPathContext.h](inc/hgl/graph/module/ShaderGenContractPathContext.h)
    - [src/SceneGraph/module/ShaderGenContractPathContext.cpp](src/SceneGraph/module/ShaderGenContractPathContext.cpp)
    - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 改为通过 `BuildShaderGenContractPathContext(...)` 统一获取 mode/policy/diff_log_detail/request/mirror 指针与 prebuild 状态。
  - `CreateMaterialWithContract` 主体已进一步收敛为 orchestrator：内部内联流程拆分为 `BuildShaderModulesFlow(...)` 与 `BuildMaterialBindingsFlow(...)` 两段静态流程函数，分别负责模块构建与 vertex/descriptor 绑定决策。
  - 上述两段 flow 已进一步迁移到独立适配器文件：
    - [inc/hgl/graph/module/MaterialBuildFlowAdapter.h](inc/hgl/graph/module/MaterialBuildFlowAdapter.h)
    - [src/SceneGraph/module/MaterialBuildFlowAdapter.cpp](src/SceneGraph/module/MaterialBuildFlowAdapter.cpp)
    - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 已删除本地实现并改为调用 adapter。
  - read-only validation 入口已独立为 gate 适配器：
    - [inc/hgl/graph/module/ShaderGenReadOnlyValidationGate.h](inc/hgl/graph/module/ShaderGenReadOnlyValidationGate.h)
    - [src/SceneGraph/module/ShaderGenReadOnlyValidationGate.cpp](src/SceneGraph/module/ShaderGenReadOnlyValidationGate.cpp)
    - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 改为统一调用 `RunReadOnlyValidationGate(...)`。
  - finalize 阶段计划生成已抽离为独立适配器：
    - [inc/hgl/graph/module/MaterialFinalizeFlowAdapter.h](inc/hgl/graph/module/MaterialFinalizeFlowAdapter.h)
    - [src/SceneGraph/module/MaterialFinalizeFlowAdapter.cpp](src/SceneGraph/module/MaterialFinalizeFlowAdapter.cpp)
    - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 改为先 `BuildMaterialFinalizePlan(...)` 再应用 `pipeline/mp/mi` 初始化。
  - 前置输入/缓存短路检查已抽离为独立适配器：
    - [inc/hgl/graph/module/MaterialCreatePrecheckAdapter.h](inc/hgl/graph/module/MaterialCreatePrecheckAdapter.h)
    - [src/SceneGraph/module/MaterialCreatePrecheckAdapter.cpp](src/SceneGraph/module/MaterialCreatePrecheckAdapter.cpp)
    - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 改为统一调用 `RunMaterialCreatePrecheck(...)` 处理 cache hit 与输入合法性短路。
  - `MaterialManager` 内 finalize 应用步骤已收敛为私有成员 `ApplyMaterialFinalizePlan(...)`（调用 `BuildMaterialFinalizePlan` 并统一应用 `pipeline/mp/mi`），`CreateMaterialWithContract` 主体进一步缩短。
  - `MaterialManager` 新增私有缓存查询助手 `TryGetCachedMaterial(...)`，替代 `CreateMaterialWithContract` 内联查询 lambda，进一步强化“阶段编排 + helper 调用”的函数形态。
 2026-03-04 运行态状态更新（用户侧 VS Rebuild 复核）
  - 用户在 Visual Studio 完整 Rebuild 后反馈：示例均可正常运行、画面正常。
  - 当前“VSCode 直接启动返回错误”更可能与启动链路/调试环境差异相关，暂不再将其单独归因到 ShaderGen contract 主路径逻辑错误。
  - 仍保留“偶发日志报错”作为后续清理项，并继续以关键 pattern 统计做回归守卫。

- 2026-03-04 头文件解耦收敛（渲染公共头去除 ShaderGen 实现依赖）
  - [inc/hgl/graph/module/MaterialManager.h](inc/hgl/graph/module/MaterialManager.h) 已移除对 `MaterialCreateInfo.h` 的直接 include，改为 `ShaderCreateInfo/ShaderCreateInfoMap` 与 contract 类型前置声明。
  - [inc/hgl/graph/module/MaterialCreatePrecheckAdapter.h](inc/hgl/graph/module/MaterialCreatePrecheckAdapter.h) 已移除对 `ShaderCreateInfoMap.h` 的直接 include，改为前置声明；具体依赖下沉到 [src/SceneGraph/module/MaterialCreatePrecheckAdapter.cpp](src/SceneGraph/module/MaterialCreatePrecheckAdapter.cpp)。
  - [inc/hgl/graph/mtl/Material3DCreateConfig.h](inc/hgl/graph/mtl/Material3DCreateConfig.h) 已将 `FixedMaterialDef.h` 依赖替换为中立头 `SkyLight.h`，避免 mtl 公共配置头经由 ShaderGen 头链传递耦合。
  - 新增中立契约入口 [inc/hgl/graph/mtl/BindingContract.h](inc/hgl/graph/mtl/BindingContract.h)，渲染侧公共头已改为通过该入口引用 binding contract：
    - [inc/hgl/vk/VKMaterial.h](inc/hgl/vk/VKMaterial.h)
    - [inc/hgl/ecs/systems/render/RenderDescriptorBindingSystem.h](inc/hgl/ecs/systems/render/RenderDescriptorBindingSystem.h)
  - 当前 `DescriptorBindingContract` 的 `shadergen` 直接 include 仅剩 ShaderGen 内部入口 [inc/hgl/shadergen/MaterialCreateInfo.h](inc/hgl/shadergen/MaterialCreateInfo.h)，渲染/ECS 公共头已无该直连路径。
  - 已完成 `DescriptorBindingContract` 归属迁移：新增 canonical 头 [inc/hgl/graph/mtl/DescriptorBindingContract.h](inc/hgl/graph/mtl/DescriptorBindingContract.h)，旧路径 [inc/hgl/shadergen/DescriptorBindingContract.h](inc/hgl/shadergen/DescriptorBindingContract.h) 已改为兼容转发（避免一次性破坏存量 include）。
  - 新增中立描述符条目定义 [inc/hgl/graph/mtl/FixedDescriptorEntry.h](inc/hgl/graph/mtl/FixedDescriptorEntry.h)，并将 `DescriptorKind/FixedDescriptorEntry` 从 [inc/hgl/shadergen/FixedMaterialDef.h](inc/hgl/shadergen/FixedMaterialDef.h) 下沉到该中立头。
  - [inc/hgl/graph/mtl/DescriptorBindingContract.h](inc/hgl/graph/mtl/DescriptorBindingContract.h) 的 `BuildBindingContract(...)` 已去除对 `FixedMaterialDef` 直接依赖，改为消费 `(FixedDescriptorEntry*, count)`；调用点 [src/ShaderGen/MaterialCompiler.cpp](src/ShaderGen/MaterialCompiler.cpp) 已同步更新。
  - 继续下沉纯布局类型：新增 [inc/hgl/graph/mtl/FixedVertexEntry.h](inc/hgl/graph/mtl/FixedVertexEntry.h) 与 [inc/hgl/graph/mtl/FixedMaterialDef.h](inc/hgl/graph/mtl/FixedMaterialDef.h)，`FixedVertexEntry/FixedMaterialDef` 数据结构已迁入中立层。
  - [inc/hgl/shadergen/FixedMaterialDef.h](inc/hgl/shadergen/FixedMaterialDef.h) 已改为聚合中立布局头 + 保留 `ShaderPermutationKey` 等生成侧语义，向“数据契约与生成逻辑分层”继续收敛。

当前阻塞：

  - VSCode 直接启动链路偶发返回错误（含历史 `exit code = 1/-1073741819`），但用户在 Visual Studio Rebuild 后示例可稳定运行且画面正常；当前优先级调整为“环境/启动链路差异排查 + 日志噪声治理”，不再作为主阻塞。
  - Phase 3 仍未完成“descriptor/pipeline layout 全量由 contract 结果驱动构建”，当前仍保留 legacy MDI 作为主来源并做一致性守卫。

下一步（建议本周）：

  1. Phase 3：推进主路径切换开关（默认走 `ShaderGenResult`，legacy 保留 fallback）并补齐回退策略验证
  2. 在现有 `StrictGate` 单测基础上，补齐 `MaterialManager::CreateMaterial` 入口级端到端严格失败路径回归用例
  3. 对 VSCode 启动链路差异与偶发日志报错做专项排查并沉淀最小复现清单（与 VS Rebuild 结果对照）
  4. 按材质/stage 聚合输出 validation/profiler（日志通道或可视化入口）

  ### 0.1 换机接手清单（可直接执行）

  建议在新机器按以下顺序接手：

  1. **先验证工程状态**
    - 打开工程后先执行目标枚举，确认 CMake Tools 正常：`ListBuildTargets_CMakeTools`
    - 若仍异常，先修复 preset/toolchain/VS 组件，再进行代码回归

  2. **先做最小回归（高优先）**
    - 抽样构建 4 个示例目标（Environment/Basic/Gizmo/Geometry 各 1 个）
    - 运行 `test_ShaderGenPathMode`
    - 目标：确认“22 文件入口迁移”未引入编译或运行时回归

  3. **再做覆盖确认（中优先）**
    - 全局检索 `int os_main(int, os_char**)`（应为 0）
    - 全局检索 `RunFramework<...>(..., width, height)` 老调用形态，确认是否仍有未透传 `argc/argv` 的示例

  4. **最后推进功能性下一步**
    - 继续完成 diff 日志通道聚合（按材质/stage）
    - 为 `mirror-preferred` 增加端到端行为回归（严格模式失败路径）

  ### 0.2 当前代码状态结论（交接摘要）

  - 已完成：Contract/Mirror/Adapter/PathPolicy/CLI 注入链路贯通，Adapter API 与内部职责收敛，全量构建回归恢复正常。
  - 未完成：Phase 3 主路径切换与运行态退出码问题定位。
  - 风险等级：**中低**（编译链路稳定；主要风险转为运行态与主路径切换验收）。

---

## 1. 结论（先答复）

结论：**可行**，而且从当前代码结构看，已经具备拆分基础。  
你提出的目标模型可定义为：

- 渲染器只提交 `需求结构（Request）`
- ShaderGen 输出：
  - descriptor 信息（set/binding + type + stage）
  - vertex input 信息
  - UBO/SSBO 结构信息
  - 按 stage 的 SPV

这在工程上可落地，但需要先把当前 ShaderGen 中对渲染器/Vulkan实现细节的直连依赖改为“契约对象 + 适配器”。

---

## 2. 现状耦合点（为什么现在还没彻底分离）

当前主要耦合来自：

1. ShaderGen 侧直接引用渲染层类型
   - `MaterialCreateInfo` 里直接出现 `VulkanDevAttr`、`UBODescriptor/SSBODescriptor`
   - `ShaderCreateInfo` / `ShaderDescriptorInfo` / `MaterialDescriptorInfo` 依赖 `VK*` 描述符与 stage 枚举

2. 渲染器直接消费 ShaderGen 内部对象
   - `MaterialManager` 直接拿 `MaterialCreateInfo`、`ShaderCreateInfoMap`、`MaterialDescriptorInfo`、`VIAArray`
   - 这意味着 ShaderGen 产物是“类对象接口”，不是“稳定 DTO/契约”

3. 编译和生成流程仍与当前图形后端语义混在一起
   - 例如 descriptor 结构布局与 Vulkan 类型在同一层

---

## 3. 目标架构（分离后）

采用 **Contract + Engine + Adapter** 三层：

- `shader-contract`（纯契约层）
  - 只放 POD/DTO、枚举、错误码、版本号
  - 不依赖渲染器对象，不依赖具体设备类

- `shadergen-core`（纯生成层）
  - 输入 `ShaderGenRequest`
  - 输出 `ShaderGenResult`
  - 负责逻辑校验、布局生成、源码组装、SPV 编译

- `renderer-adapter`（渲染器适配层）
  - 把 `ShaderGenResult` 映射到 `MaterialDescriptorManager / PipelineLayout / VertexInput`
  - 这是唯一了解渲染器内部资源管理的层

---

## 4. 建议契约（最小可用）

> 下面是建议形状，不要求一次到位；先建立字段与责任边界。

### 4.1 输入（Renderer -> ShaderGen）

```cpp
struct ShaderGenRequest {
    uint32_t contract_version;

    // 材质与变体
    MaterialPreset material_id;
    MaterialCreateConfigLite material_cfg;
    ShaderPermutationKey permutation;
    PipelineMode pipeline_mode;

    // 资源需求（渲染器声明“我需要什么”）
    Span<const ResourceRequirement> required_resources;

    // 顶点输入需求
    Span<const VertexInputRequirement> vertex_requirements;

    // 平台/质量档
    PlatformTier platform_tier;
    QualityLevel quality_level;

    // 编译选项
    bool enable_debug_info;
    bool enable_fallback;
};
```

### 4.2 输出（ShaderGen -> Renderer）

```cpp
struct ShaderGenResult {
    uint32_t contract_version;

    // 1) SPV
    Span<const StageSpvBlob> spv_per_stage;

    // 2) 资源布局
    ShaderResourceLayout layout;         // descriptor set/binding/type/stage

    // 3) 顶点布局
    VertexInputLayout vertex_layout;     // location/type/rate/group

    // 4) 结构体信息（UBO/SSBO）
    Span<const BufferStructDesc> buffer_structs;

    // 5) 诊断
    ShaderDiagnostics diagnostics;

    // 6) 缓存键
    ShaderCacheKey cache_key;
};
```

### 4.3 关键原则

- Contract 层禁止出现 `VulkanDevAttr`、`VulkanDevice`、`MaterialManager` 等对象
- `VIAArray/SVArray` 作为内部专用结构可保留在 core，但**对外只暴露 DTO**
- 字符串对外统一 `std::string` / `string_view`

---

## 5. 分阶段重构计划

## Phase 0：冻结边界与契约草案（1~2 天）

- 产出 `shader-contract` 初版结构定义（仅头文件）
- 标记“禁止新增耦合点”规则
- 确认与现有规范文档一致：
  - `SHADER_LOGIC_CONSTRAINTS_SPEC.md`
  - `RESOURCE_LAYOUT_BINDING_STRATEGY.md`

**验收**：契约头文件可独立编译，无渲染器 include。

## Phase 1：输出镜像（不改行为）（2~4 天）

- 在 ShaderGen 内部生成现有对象的同时，额外组装 `ShaderGenResult`（镜像）
- 渲染器暂时仍走旧接口

**验收**：旧路径 100% 不变，新增镜像与旧对象字段一致性校验通过。

## Phase 2：渲染器适配器接入（3~5 天）

- 新增 `RendererShaderGenAdapter`
- `MaterialManager` 增加新入口：从 `ShaderGenResult` 构建材质
- 保留旧入口并行（双轨）

**验收**：同一材质双轨输出一致（layout/SPV/hash）。

## Phase 3：切主路径（2~3 天）

- 默认切换到 `ShaderGenResult` 路径
- 旧 `MaterialCreateInfo` 路径保留为 fallback（编译开关控制）

**验收**：默认路径稳定运行，旧路径可回退。

## Phase 4：移除反向耦合（2~4 天）

- 从 ShaderGen public 头中移除渲染器实现对象依赖
- 旧对象接口降级为内部实现细节或删除

**验收**：ShaderGen 可作为独立静态库被非渲染模块链接（仅依赖 contract）。

## Phase 5：清理与文档统一（1~2 天）

- 删除弃用接口、更新文档索引与规范
- 增加 CI 校验：禁止新代码跨层 include

**验收**：文档、代码、CI 规则一致。

## Phase 6：独立程序化与离线产物链路（下一阶段，不在本期强制落地）

目标（North Star）：**运行时引擎不再包含 ShaderGen 与 GLSLCompiler**。

- 主引擎输出“渲染需求配置 + 画质/平台配置”到文件（本地/云端统一文件协议）
- `shadergen-cli`（独立可执行程序）读取配置文件，生成：
  - `ShaderGenResult` 材质信息包（descriptor/vertex/UBO-SSBO/diagnostics/cache key）
  - 按 stage 的 SPV 包（可拆分或聚合）
  - manifest（版本/平台/质量档/hash/依赖）
- 主引擎运行时仅加载上述产物包并创建渲染资源，不链接 ShaderGen/GLSLCompiler

建议分解：

1. `ShaderGenRequest/Result` 文件协议定版（JSON 或二进制 + schema version）
2. 新增 `shadergen-cli`：`input request file -> output package`（支持本地与云端任务）
3. 引擎新增 `ShaderPackageLoader`（消费包，不回调 ShaderGen）
4. 构建系统剥离运行时对 `ULRE.ShaderGen/GLSLCompiler` 的强依赖（仅工具链依赖）
5. CI 增加“离线包回放”验收：同一 request 在本地/云端产物 hash 一致

**验收**：示例与游戏运行路径在不链接 ShaderGen/GLSLCompiler 的前提下可启动并正确加载材质包。

### 6.1 子任务拆单（可直接排期）

#### Task A：Request/Result 文件协议定版

当前草案（已落地，可评审）：

- [doc/shader-system/schema/shadergen-request.schema.json](doc/shader-system/schema/shadergen-request.schema.json)
- [doc/shader-system/schema/shadergen-result.schema.json](doc/shader-system/schema/shadergen-result.schema.json)

- 定义 `shadergen-request.schema.json`（或二进制等价 schema）
  - `contract_version`
  - `material_id/material_cfg/permutation/pipeline_mode`
  - `required_resources/vertex_requirements`
  - `platform_tier/quality_level`
  - `compiler_options`（debug/fallback/opt-level）
- 定义 `shadergen-result.schema.json`
  - `spv_per_stage`（stage mask + blob hash + byte size）
  - `layout/vertex_layout/buffer_structs`
  - `diagnostics/cache_key`
- 增加 schema 版本升级规则（forward/backward 兼容策略）

**Task A 验收**：同一 request 在工具链中可稳定序列化/反序列化，字段无丢失。

#### Task B：Shader Package 格式与 Manifest

当前草案（已落地，可评审）：

- [doc/shader-system/schema/shader-package-manifest.schema.json](doc/shader-system/schema/shader-package-manifest.schema.json)
- [doc/shader-system/schema/examples/shader-package.manifest.example.json](doc/shader-system/schema/examples/shader-package.manifest.example.json)
- [doc/shader-system/SHADER_PACKAGE_LAYOUT_SPEC.md](doc/shader-system/SHADER_PACKAGE_LAYOUT_SPEC.md)

- 定义包目录与命名规范（示例）
  - `manifest.json`
  - `materials/<material_key>/layout.json`
  - `materials/<material_key>/vertex.json`
  - `materials/<material_key>/spv_stage_0x*.spv`
- `manifest` 必含字段
  - `package_version`
  - `contract_version`
  - `target_platform/quality`
  - `request_hash/result_hash/content_hash`
  - `generator_version/build_time`
- 增加完整性校验（hash/size）与可选签名字段（云端分发预留）

**Task B 验收**：引擎可仅凭 manifest + 包内容完成加载，不访问 ShaderGen。

#### Task C：`shadergen-cli` 工具化

当前草案（已落地，可评审）：

- [doc/shader-system/SHADERGEN_CLI_SPEC.md](doc/shader-system/SHADERGEN_CLI_SPEC.md)
- [doc/shader-system/schema/shadergen-cli-log.schema.json](doc/shader-system/schema/shadergen-cli-log.schema.json)
- [doc/shader-system/schema/examples/shadergen-cli.log.example.jsonl](doc/shader-system/schema/examples/shadergen-cli.log.example.jsonl)

- 命令行协议（建议）
  - `shadergen-cli generate --request <in> --out <dir>`
  - `shadergen-cli pack --in <result-dir> --out <package>`
  - `shadergen-cli verify --package <pkg>`
- 输出退出码与错误分类
  - `0` 成功
  - `2` 输入协议错误
  - `3` 编译失败
  - `4` 包校验失败
- 产出结构化日志（JSONL 或 key=value）便于云端采集

**Task C 验收**：本地脚本与 CI 可无引擎进程参与完成 generate/pack/verify 全流程。

#### Task D：引擎侧 `ShaderPackageLoader`

- 只依赖 contract DTO 与 package 读取器
- 加载流程：manifest 校验 -> 布局构建 -> SPV module 创建 -> pipeline 绑定
- 缺失/损坏包的 fallback 策略（按模式：严格失败或加载内置保底材质）

**Task D 验收**：运行时二进制不链接 `ULRE.ShaderGen` 与 `GLSLCompiler` 仍可渲染。

#### Task E：云端任务协议（与本地同构）

- 上传 request 文件 + 目标平台参数
- 云端返回 package + manifest + 构建日志
- 使用同一 schema/version 规则，禁止“云端私有字段漂移”

**Task E 验收**：同 request 在本地与云端生成的 `result_hash/content_hash` 一致。

### 6.2 当前阶段（Phase 3~5）对齐原则

为避免返工，当前解耦改造需持续满足：

1. **可序列化优先**：新增 contract 字段必须可稳定落盘（避免仅内存临时结构）
2. **构建可重放**：同一 request 可重复生成同一 SPV/hash（控制非确定性）
3. **运行时禁反向依赖**：新增运行时代码禁止回调 ShaderGen 编译流程
4. **失败可诊断**：所有关键失败输出 machine-readable 原因（便于云端聚合）

### 6.3 里程碑建议（下期排程）

- M1（协议周）：完成 Task A + Task B，冻结 v1 schema 与 manifest
- M2（工具周）：完成 Task C，打通本地离线 generate/pack/verify
- M3（接入周）：完成 Task D，示例运行时切换到 package-only
- M4（云端周）：完成 Task E，本地/云端 hash 一致性纳入 CI

### 6.4 MiniPack 接入规划（Shader 包统一单文件）

目标：将当前“材质信息 + SPV 散文件目录”统一封装为单个 `*.pack`，并由运行时通过 MiniPack 读取。

已分析的 MiniPack 现状（可直接复用）：

- 工具与库能力完整：已提供打包 CLI（目录/文件列表 -> 单 pack）与读写库接口
  - 入口： [src/Tools/MiniPack/main.cpp](src/Tools/MiniPack/main.cpp)
  - 写入 API： [src/Tools/MiniPack/mini_pack_builder.h](src/Tools/MiniPack/mini_pack_builder.h)
  - 读取 API： [src/Tools/MiniPack/pack_reader_io.h](src/Tools/MiniPack/pack_reader_io.h)
- 文件名以相对路径存储，天然适配 `materials/<id>/...` 结构
- 支持 `--index-only`，可用于云端预检/索引校验场景

当前与 Shader 包场景的差距（建议补齐）：

1. 需增加“按文件名快速查找 entry”能力（避免运行时线性扫描）
2. 需增加 package 级 metadata 约定（`manifest.json` 仍保留为标准入口）
3. 需增加完整性校验流程（content hash 与 entry size 校验）
4. 需确认 writer/reader 对 magic 与版本字段的一致性测试覆盖，防止跨版本读取失败

MiniPack 落地步骤（并入 Phase 6 子任务）：

- MP-1：定义 Shader 包目录规范并固定到 manifest
  - `manifest.json`
  - `materials/<material_key>/layout.json`
  - `materials/<material_key>/vertex.json`
  - `materials/<material_key>/spv_stage_0x*.spv`
- MP-2：`shadergen-cli` 输出散文件后，调用 MiniPack 统一打包为 `shader_package.pack`
- MP-3：引擎侧 `ShaderPackageLoader` 基于 MiniPack reader 加载 manifest 与目标材质 SPV
- MP-4：CI 增加“目录产物 vs MiniPack 产物”一致性回放（hash + 运行结果）

MiniPack 接入验收：

- 同一批 request 生成的散文件目录与 `shader_package.pack` 在运行时加载结果一致
- 运行时加载链路仅依赖 MiniPack reader，不依赖 ShaderGen/GLSLCompiler
- 包损坏或 manifest 不匹配时可输出 machine-readable 错误并按策略回退/失败

---

## 6. 迁移风险与控制

### 风险 A：布局偏差（set/binding 不一致）

- 控制：双轨阶段做逐材质布局 diff（文本化输出）

### 风险 B：SPV 不一致

- 控制：按 stage 比较 hash + 反汇编结构关键段

### 风险 C：性能回退

- 控制：缓存键稳定化，确保新路径命中率不下降

### 风险 D：接口震荡

- 控制：contract version 字段 + 适配器层隔离

---

## 7. 验收标准（必须满足）

1. 渲染器只依赖 `shader-contract` + `shadergen-core` 的对外 DTO
2. ShaderGen 对外 API 不再暴露渲染器实现类型
3. 结果完整覆盖你要求的 4 类产物：
   - descriptor 信息
   - vertex input 信息
   - UBO/SSBO 信息
   - SPV
4. `03_BasicLitSunDirectionECS` 与 `ULRE.ShaderGen` 构建/运行回归通过

---

## 8. 推荐落地顺序（与你当前代码最匹配）

优先从 `MaterialManager` 消费面切：

1. 先定义 `ShaderGenResult`（不动内部生成逻辑）
2. 在 `MaterialCreateInfo` 末端导出 `ShaderGenResult`
3. 新建 `RendererShaderGenAdapter` 消费 `ShaderGenResult`
4. `MaterialManager` 切换调用新适配器
5. 最后再收缩/删除 `MaterialCreateInfo` 旧暴露面

这个顺序风险最低，因为先“加新桥”，再“撤旧桥”。

---

## 9. 建议新增文件（执行期）

- `inc/hgl/shadergen/contract/ShaderGenContract.h` ✅（已创建）
- `src/ShaderGen/contract/ShaderGenResultBuilder.cpp`
- `inc/hgl/graph/module/RendererShaderGenAdapter.h`
- `src/SceneGraph/module/RendererShaderGenAdapter.cpp`
- `doc/shader-system/SHADERGEN_RENDERER_DECOUPLING_PLAN.md`（本文）

---

## 10. 最终建议

- **是可行的**，而且值得做。  
- 建议采用“**双轨镜像 -> 适配器切换 -> 拆除旧耦合**”路线，不要一步到位硬切。  
- 保持你现有规范（Logic/Binding/Whitelist）不变，只重构边界与交付形态。
- 将“独立程序化（文件进/文件出）”作为下一阶段总目标：当前解耦阶段优先保证 contract 可序列化、产物可复现、运行时仅消费包。

---

## 11. 后续重构提醒（暂不在本期处理）

- 当前为修复 gizmo 子世界渲染链路，临时在子世界路径执行了相机/UBO 同步。
- 这暴露了架构设计问题：**子世界（SubWorld）不应持有独立 System 集合**，子世界应主要负责世界组成与实体组织。
- 后续需要在 ECS 层进行专项重构：
  - 重新划分 `Context` 与 `World` 职责边界；
  - 将 System 生命周期与调度收敛到主世界/主上下文；
  - 为 SubWorld 提供轻量的“上下文继承/视图”机制，避免重复 System 与状态分叉。
- 本项作为后续架构任务保留，**不纳入当前阶段修复范围**。
- 独立任务卡：`doc/ECS_SUBWORLD_CONTEXT_WORLD_REFACTOR_TASK.md`（用于后续排期与验收追踪）。
