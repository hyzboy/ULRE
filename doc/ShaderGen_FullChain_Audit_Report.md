# ShaderGen 全链审计报告（L1-L4 四路扫描 + 处置状态回溯）

日期：2026-08-16/17 | 范围：从材质定义（L1 作者层）→ 模块系统（L2）→ 生成层（L3）→ 契约/运行时（L4）整条工作链
方法：4 路并行子代理（各 ~360s）+ 独立抽查复核；全部"0 使用"经全仓 grep 实证；行号基于扫描时磁盘状态，处置后可能漂移。

---

## 一、总览

四层共发现 **高 21 / 中 33 / 低 30** 项。经本轮工作链（T1-T4 + 死代码阶段 1-3 + ECS 侧无关）处置：

- **已删除/已收敛：~700 行**（死常量、condition 系统、metadata_version 轨道、版本/schema 常量堆叠、ForceLink hack、extra_attributes、UV1、3D 前缀、generate_only 改名）
- **已决策不修（MeshShader 方向）：顶点侧全部**（输入反推、TerrainGrid 特判、语义词汇归一、pass/purpose 单源化、GenerateVertexShader 回退分支、2d/3d 目录）
- **已决策保留：验证载体**（ResolvedModuleGraphBuilder/CapabilityResolver 层——回归门 stage key 哈希依赖；磁盘缓存不接线）
- **剩余可选：高 5 / 中 12 / 低 15**（见第四节）

## 二、已处置项（按提交回溯）

| 发现 | 处置 | 提交 |
|---|---|---|
| L1 死 bootstrap 枚举（ErrorCheckerboard/PureDepth）+ BUILTIN_MTL_DEF_FALLBACK 同值重复 | 删枚举 + FALLBACK 合并入 PURE_COLOR（含 TOML 解析映射、回归门断言同步） | `4086ad5f0` |
| L1 ForceLink 机制（空函数 + 静态初始化序 + 链接器 dead-strip hack） | 改惰性注册（真实注册函数 + 无环 Ensure——**执行中抓到函数内 static 死锁**并修复） | `c0c1f1125` |
| L3 generate_only 命名/文档三义（实为生产主路径"延迟 finalize"） | 改名 `defer_finalize` + 注释修正（9 处） | `c0c1f1125` |
| L2 R3 condition 系统（3 domain × 2 operator 从未求值，只改哈希不改行为） | 全链删除（解析/校验/哈希/契约字段/provider 冲突规则简化） | `dfdc35e76` |
| L2 R1 metadata_version 轨道（55 模块 0 使用，全 unversioned 特例） | 全链删除（常量/字段/version 指令/uses 版本区间/requires 门/区间检查/4 错误码） | `dfdc35e76` |
| L4 R1 版本/schema 常量堆叠（8 组：契约/Key/Artifact/profile/manifest 全"全局失效开关"） | 全删（**执行中抓到 ValidateDescriptorContract 误改 bug**） | `dfdc35e76` |
| L1 "3D" 前缀（唯一描述符构建器被全部材质共用，含 2D） | 改名 DefinitionDescriptorBuilder（git mv + 类型/函数/引用全同步） | `3a8c4c0ca` |
| L3 extra_attributes 死字段（恒空）+ L1 UV1 死值 + L1 死常量 3 个 | 全链删除（plan 字段/签名/emit 分支/调用点/回归门断言） | `4086ad5f0` |

## 三、已决策项（不修/保留，附理由）

| 发现 | 决策 | 理由 |
|---|---|---|
| L2 R2 ResolvedModuleGraphBuilder 整层生产未接线（~720 行 + 序列化） | **保留不接线** | 回归门约 9 组（J/M/I2/O/P/Q/Q1/MI）依赖其 stage key 哈希验证；MeshShader 时代 SPV 缓存体系仍需要该验证地基；删=拆验证地基无收益 |
| L4 S1 磁盘程序缓存（ShaderArtifactStore）回归门专用、生产 0 实例 | **不接线** | 决策点 2：顶点侧转型前磁盘缓存零投入 |
| L1 顶点 input 反推（position_format→effective_input 覆盖显式声明） | **不修** | 顶点侧过渡资产（VertexShader+VBO 将废弃），"保持简单能过即可" |
| L1/L3 TerrainGrid 特判（产出"能编译但全零"shader） | **不修** | 同上（顶点 mapping 模式，无生产使用）；若未来真的使用需先实现 |
| L1/L3 GenerateVertexShader 无 resolved 回退分支（latent 不可编译 bug） | **不修** | 仅回归门触发且只做字符串断言；生产恒走完整路径 |
| L4 N1/N4、L2 N4 语义词汇五套 / pass-purpose 双轨双向映射 | **不修** | 顶点侧词汇随 VBO 废弃；pass/purpose 单源化与顶点侧关联 |
| L3 use_resolved_render_state 默认值 false | **不修** | 双路径都活（生产恒 true、回归门简版依赖默认 false）——误删已回滚，禁止再动 |
| L3 双轨编译（ShaderProgramManager:419 全量编译取 schema） | **不修** | 低收益；生成链重构时顺带 |
| L4 N11 回归门 ULRE_REPO_ROOT vs 运行时环境变量双轨 | **保留** | 回归门确定性（编译期宏）与运行时定位（env/搜索）各有用途 |

## 四、剩余项（按优先级）

### P1 小项清理（低风险，随时可做）—— ✅ 已完成（提交 00c18411f/a1fb0fb28，除 12 跳过）

1. ✅ **L1【高】SamplerName.h 死文件**——已删
2. ✅ **L1【高】MaterialDefinitionUsageTag 死字段**——已删（枚举/字段/校验）
3. ✅ **L3【低】plan.render_state 死字段**——已删
4. ✅ **L4【高】S3 fallback 静默降级**——已改显式 GLogError 诊断 + 删死参数
5. ✅ **L4【中】S8 双级脏检查不含 purpose**——已纳入 build_context_hash（哈希敏感：确定性验证通过）
6. ✅ **L2【低】S8 死代码批**——ParseDescriptorSemantic/name_index/missing_provider 已删；错误码错位已修（P3-2）
7. ✅ **L2【低】S6 文件名→模块名回退**——已删（name 强制必填）
8. ✅ **L1【中】别名机制**——已删（Text2D 用规范 ID；MISSING_MATERIAL 合并 pure_color）
9. ✅ **L1【低】backup/**——已移出仓库（E:/ULRE_external_backup/）
10. ✅ **L2【低】R6 过期注释 + reserved 死字段**——已删
11. ✅ **L4【中】S9 MaterialTexture/Sampler 契约**——已加显式 case（bindless 通道）
12. ⏭️→**C1** **L4【低】S7 TextureSlot::BaseColor 默认**——跳过（23 回归门断言+26 生产读取依赖，随数据化处理）

### P2 结构收敛（中风险，建议独立批次逐项验证）—— ✅ 已完成（提交 f36c6c212/c541f4781/43ea9e79d）

13. ✅ **L3【高】pipeline_state_hash 恒 0**——已删（43ea9e79d）
14. ✅ **L3【高】TBuiltInResourceCompat ABI**——接口加 abi_version 校验 + 插件加载 exe 目录回退（f36c6c212）
15. ✅ **L2【中】R4/R5**——GetNormalized 内联（公共访问器保留）+ 校验层接线 LoadDirectory（f36c6c212）
16. ✅ **L2【中】S5 跨目录重试**——已删（while 收敛保留——连锁剔除正确性）（c541f4781）
17. ✅ **L2【中】S3/S4 空转词汇**——Basis/Decode/flags/ubo/Exclusive 全删（c541f4781）
18. ✅ **L4【中】N8 sampler**——GetIndex 显式无效值 + 宏生成报错（43ea9e79d；双解析器本就单一 SamplerPresetLibrary——报告过时）
19. ✅ **L3【中】字符串手术批**——显式宏对 + SSBO buffer 显式表 + 注释（43ea9e79d；双 marker 回退留转型期）
20. ✅ **L4【中】N5 双 profile 哈希**——统一编译目标超集（43ea9e79d）；N6 手写 JSON 保持（0→1 无第三方依赖）

### P3 大工程（MeshShader 转型期一并做）—— 🟡 已评估（提交 88d27779d：21=无需（T4 已统一）、22=文档化+错误码、23=无需（报告过时）、24-28 未做——标记见下）

21. ✅ **L2/L3 S1 模块路径**——无需执行（T4 已统一 GetShaderLibraryPath 全仓唯一入口；coverage dispatch 留转型期）
22. ✅ **L2 N2/N3 三份哈希**——已文档化语义边界 + 错误码修正（88d27779d；计算本就单源）
23. ⏭️→**B11** **L1 四结构重叠**——评估：现存三形态互补非重叠（报告基于已清理旧代码）；概念层 MaterialBindingContract vs DescriptorContract 可再查
24. ⏭️→**B3** **L1 DescriptorSemantic/UBODescriptorSemantic 双枚举**——未做（UBO 解析链已删（c541f4781）；哈希敏感）
25. ⏭️→**B2** **L1 Schema/Manifest 命名**——未做（Manifest→ShaderCodeResourceManifest + 目录/命名空间错位 inc/hgl/graph/glsl 但 namespace mtl）
26. ⏭️→**B1** **L4 N2 Binormal→Bitangent**——未做（哈希敏感：语义名进哈希，需全链一次性替换+零漂移验证）
27. ⏭️→**B4** **L4 S6/S10/S4 coverage 数据化**——未做（声明式裁剪：S6 契约数据化 + S10 输出白名单 + S4 depth 布尔提显式相位）
28. ⏭️ **L1 双套顶点构造入口**——未做（顶点侧，随 VBO 废弃——保持）

## 五、验证纪律（已固化为方法论）

- 每步：全量构建 0 错误 + 回归门 43 PASS + 示例运行（exit=124）
- 哈希触碰任务：删缓存跑两遍、stage key 清单 IDENTICAL（确定性验证）
- 纯删除任务：stage key 与基线零漂移
- 改公共头 POD 布局：清中间目录全量重编（MSB8028 坑，段错误只在运行期暴露）
- **示例串行 + sleep 2 间隔**（AMD 设备释放延迟——回归门/上一示例后首启偶发 127/139 假崩溃，重跑即正常）
- **grep 使用面必须含 example/**（两次误删教训：SyncSkyUBO、RenderDrawOnly——"0 调用"判定 = 全仓含 example/ 的 grep）
- **头定义→cpp 迁移后检查依赖库 obj 重编**（LNK2005 新形态：`already defined in ULRE.Work.lib(WorkManager.obj)`——rm build/example 无效，须 rm -rf build/src/* 全量重编）

## 附：四层报告原文位置

`C:\Users\hyzbo\AppData\Local\hermes\cache\delegation\subagent-summary-{0..3}-20260816_194005_{717233,718233}.txt.plain.md`
（L1 作者层 / L2 模块系统 / L3 生成层 / L4 契约运行时；方法论与实战教训已固化于 ulre-cm-modules 技能 shadergen-* 系列）

---

## 六、ECS 联动清理（2026-08，独立于 ShaderGen 审计）

同轮完成的 ECS 渲染架构清理（hgl::ecs），累计约 **-3500 行**，回归门全程 43 PASS：

| 阶段 | 提交 | 内容 |
|---|---|---|
| P0 功能 | 5bf2d696e | Tick 断线接线 + ICB 命令偏移 + needs_indirect 守卫 |
| P1 死代码 | 56938d038..d61da60ef | TAB 旧路径/查询缓存整套/SystemType/序列化 8 API（-2100 行） |
| P2 双轨 | 3061691df | 系统组 EnsureSystemGroupsRegistered 幂等（零重扫） |
| P3 形态 | 3012d626b | Text 去 Adapter 直接继承 RenderPipelineBase |
| 审计项 | 88d27779d | 哈希标识文档化 + 错误码修正 |
| W2 帧入口 | a0dff93d0 | 6 Render 变体→单驱动；删 World 死层类 |
| W3 初始化 | 07af1413d + 1e694f34e | 双 Initialize 合并；RenderDrawOnly 恢复（离屏真用例） |
| W6 相机 | cb5d6560e | SyncCameraUBO 脏驱动单写点 |
| W5 变换 | ab168b89e | SubmitTransformUpdates 帧级 once（ring 语义评估修正） |
| W4 绑定表 | f9f311a93 | LocalToWorldIndexTable 4 级→3 级 fallback |
| W7 命名 | 4044a679b | SetExecutionPhase/GetEntityID/UBOSyncSystem/管线名统一/RenderResource 上提 |
| 回归修复 | e060ebfb4 + 7acaae8ca | W7 GetID 改名漏改 8 处（6 调用点 + CreateChildEntity 封装内部 + LoadGeometry 示例）——gizmo 视觉 asset 层级断裂根因；澄清 BasicLitSunDirection「最外圈不面向屏幕」实为该层级断裂（旋转环 aux_transform 经 CreateChildEntity 挂载），非 7.19 删除 Billboard 代码，无需恢复 |

ECS 侧剩余（P4，MeshShader 转型时）：archetype/SoA 组件存储、GatherSceneStats 增量计数、
绑定链脏门控（每帧日志限流已随 fd184b8bd + TransformComponent/TransformSystem 计数器限流完成）。
详细坑见技能 `ulre-cm-modules` 的 ecs-* 系列 references。
