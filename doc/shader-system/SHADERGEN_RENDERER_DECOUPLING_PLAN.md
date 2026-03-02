# ShaderGen 与渲染器彻底分离重构计划（可行性 + 执行方案）

**版本**：v1.2  
**日期**：2026-03-02  
**状态**：执行中（Phase 2 基本落地，待构建回归）

---

## 0. 当前进展（2026-03-02）

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
- Phase 2 并行入口已增加
  - [src/SceneGraph/module/MaterialManager.cpp](src/SceneGraph/module/MaterialManager.cpp) 新增 contract-aware 私有入口：可显式接收预构建 `ShaderGenResult`
  - 现有 `CreateMaterial` 已改为“先尝试预构建 mirror result，再转发到并行入口”，为后续主路径切换预留开关点
  - 新增 `ShaderGenRequestBuilder`（`MaterialCreateInfo -> ShaderGenRequest`）并接入 `MaterialManager`，形成 Request/Result 双 DTO 并行链路
  - `RendererShaderGenAdapter` 新增 `ConsumeRequestResultReadOnly(...)`：对 `required_resources` 与 `vertex_requirements` 做 request/result 一致性校验（non-blocking）
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

当前阻塞：

  - 本机 CMake Tools 在执行构建时返回空错误（result code = -1 且无 stdout/stderr），导致本轮“最后 22 文件迁移”的构建回归暂未完成；代码级静态错误已清零。

下一步（建议本周）：

  1. 在新机器恢复 CMake Tools 构建能力并完成示例抽样 + `test_ShaderGenPathMode` 回归验证
  2. 在验证通过后，补齐剩余示例入口透传覆盖（如仍有漏网）并做一次全量检索确认
  3. 将 diff 输出接入可筛选日志通道（按材质/阶段聚合）
  4. 将 `ShaderGenPathPolicy` 挂接到更高层配置源（命令行/配置文件），由应用启动阶段一次注入

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

  - 已完成：Contract/Mirror/Adapter/PathPolicy/CLI 注入链路贯通，示例入口大面积透传。
  - 未完成：最后一批入口改动后的构建回归（受本机 CMake Tools 异常阻塞）。
  - 风险等级：**中低**（代码侧静态错误已清零；主要风险在“未完成最终编译验收”）。

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
    InlineMaterial material_id;
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
