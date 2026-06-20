# ShaderGen 重做技术总设计（主文档）

## 1. 文档目的

本文件用于指导从当前稳定提交重新实施 `recfactor/ShaderGen_MaterialPresetTable` 分支意图，目标是：

1. 保留真正有收益的架构改进。
2. 降低过渡期并行双轨造成的失稳风险。
3. 保证每个阶段结束时，示例程序持续可运行。
4. 为实现人员提供无歧义执行说明，避免“理解偏差导致错误落地”。

本主文档是唯一总控文档。阶段实施细节见阶段子文档。

---

## 2. 基线与目标

### 2.1 当前基线

- 当前工作基线：`HEAD` 位于稳定提交（示例可运行）。
- 目标对照分支：`recfactor/ShaderGen_MaterialPresetTable`。
- 该分支相对当前 `HEAD`：约 103 commits，约 287 文件变更，代码量变化约 +10376 / -7724。

### 2.2 最终目标（重做后）

1. 运行期 shader 路由由 `MaterialPresetTable + Matcher` 统一主导。
2. fallback 限定在同 preset 的候选集合内，禁止跨 preset 静默回退。
3. `GeometryMode` 从运行期关键路径退出，减少错配源。
4. `kBuiltinVariants[]` 与 `PresetDemandTable` 从 runtime 路由退出。
5. 保持可演进能力：后续可安全接入 overlay/pass resolver，而不强绑在本轮。

---

## 3. 设计原则（强约束）

1. 阶段内闭环：一个阶段内引入的能力必须在该阶段内可验证、可回滚。
2. 禁止半迁移：禁止出现“新路径定义了但没有主路径调用”。
3. 先桥接后替换：先把新能力接入运行路径，再删除旧能力。
4. fallback 可解释：所有 fallback 行为必须可记录原因，禁止 silent fallback。
5. 兼容优先：本轮不以“结构最优”优先，而以“稳定可落地”优先。

---

## 4. 范围定义

### 4.1 本轮纳入

1. 顶点策略解耦：`2D->3D` 与 `旋转扭正` 分离为独立策略轴。
2. Billboard/Quad 专有路径退场（迁移到通用策略组合）。
3. ColorSource 新通路接入（先并存，再替换）。
4. SFM 注解与 requirement 解析自检。
5. MaterialPresetTable + Matcher + MatchedShaderSet 主路径接入。
6. `GeometryMode` 清理与 key 简化。
7. 旧 runtime 路由表退场与收尾清理。

### 4.2 本轮不纳入（延后）

1. 全量 `MaterialRecipe` 三段式重构（DrawSpec/ResourceSupply/AlphaConfig）的大范围调用点改造。
2. `ShaderUBOEmitter` 替换全部 UBO include 的体系重写。
3. OverlayChannel / PassShaderResolver 的完整运行期注入闭环（只保留扩展位）。
4. 与多平台（TargetPlatform）耦合的扩展。

---

## 5. 里程碑与阶段拆分

| 阶段 | 名称 | 核心目标 | 产物 | 风险等级 |
|---|---|---|---|---|
| Phase 0R | VertexPolicy 解耦 | 将 `2D->3D` 与 `旋转扭正` 拆分成独立策略轴 | 可组合策略模型 + 兼容桥接层 | 中 |
| Phase 0 | Billboard/Quad 退场 | 删除 Billboard/Quad 专有 preset 与专用路径 | 全部迁移到通用策略组合 | 中低 |
| Phase 1 | ColorSource 接入 | CodegenRegistry 并入主流程（保留旧路） | 双路并存可切换 | 中低 |
| Phase 2 | SFM 与 PositionProvider | requirement 自描述 + lint + provider 扩展 | 可审计的 shader 需求 | 中 |
| Phase 3 | Matcher 主路由 | PresetTable + Matcher 接入 runtime 主路由 | MatchedShaderSet 生效 | 高 |
| Phase 4 | Key/Geometry 清理 | 删除 GeometryMode 关键依赖，精简 key | 路由轴稳定 | 中高 |
| Phase 5 | 旧路径退场 | kBuiltinVariants/PresetDemandTable 退出 runtime | 单一路由架构 | 中 |

---

### 5.1 当前执行状态（2026-06-20）

1. Day 1（Phase0R 骨架）已完成并通过窄验证：
   - 四轴类型与 recipe 字段接入（`VertexSourcePolicy/GeometryLiftPolicy/OrientationPolicy/SizePolicy`）。
   - Quad/Billboard 旧入口完成四轴桥接映射（保持旧 `vertex_policy` 行为）。
   - `RecipeToKey` 增加 Phase0R 策略解析与本地校验 API，补齐最小 VT 用例。
   - `MaterialRecipeStore` 内容哈希已覆盖四轴字段，避免策略差异被误去重。
2. 本阶段验证结果：`RecipeToKeyTest`、`MaterialRecipeStoreTests`、`01_Billboard` 均已通过。
3. 已知遗留项（不阻塞进入 Phase2）：
   - billboard 顶点着色器仍存在固定像素尺寸常量路径（未完成 SizePolicy 资源化消费）。
   - 当前链路尚未为 billboard size 提供专用 UBO/SSBO 数据源。
4. 阶段切换决议：即刻进入 Phase2（SFM 与 PositionProvider），同时将上述遗留项作为并行跟踪项保留在 Phase0R 收尾列表中。

---

## 6. 全阶段统一术语与契约

### 6.1 核心对象

1. `MaterialPresetTable`：按 `Preset x QualityLevel x RenderPhase` 给出候选 surface（及可选 VS/FS override）。
2. `Matcher`：对候选执行能力匹配（SFM requirement 集合包含），并做同 preset 降级。
3. `MatchedShaderSet`：Matcher 输出，作为 runtime 材质创建输入桥。
4. `SFM`：GLSL 顶部注解，声明 require/optional/derive/supports_phase/surface_type。

### 6.2 Vertex Source 分层模型（新增）

运行时顶点处理统一为四层：

1. Vertex Source Layer：`VBO3DDirect` / `VBO2DTo3D` / `PCG`。
2. Geometry Lift Layer：`2D->3D` 升维规则（如 `XY_To_XY0` / `XY_To_X0Y`）。
3. Orientation Layer：朝向/扭正（如 `FaceCameraFull` / `FaceCameraAxisLocked`）。
4. Size Layer：世界尺寸/像素尺寸/混合尺寸。

强约束：Layer2 输出后统一为 Local3D，Layer3/Layer4 不允许再按“来源是 2D 还是 3D”分支。

### 6.3 fallback 契约

1. 只允许在当前 preset 候选行内降级。
2. 失败必须进入 `CheckerboardFallback` 或显式错误。
3. 禁止回退时跨 preset 自动选中别的 surface。

### 6.4 日志契约

至少输出：

1. 输入 recipe 的关键轴（preset、phase、quality、position provider、VertexSourcePolicy、GeometryLiftPolicy、OrientationPolicy、SizePolicy）。
2. 候选列表与每个候选失败原因（缺少 VA/tex/ubo/ssbo/phase 不支持）。
3. 最终命中候选，或 fallback 原因。

统一 `LogCode` 前缀规范（全阶段共享）：

1. `VT-OK-*`：组合真值表中的合法命中路径（对应 `VT-001`~`VT-099`）。
2. `VT-ERR-*`：组合真值表中的非法组合或资源缺失（对应 `VT-101` 起）。
3. `MT-MATCH-*`：Matcher 候选匹配过程日志（候选筛选、能力不满足、同 preset 降级）。
4. `MT-FALLBACK-*`：显式 fallback 路径（包括 `CheckerboardFallback` 触发原因）。

编码规则：

1. 一个决策路径至少包含 1 条阶段级 `LogCode`（`VT-*` 或 `MT-*`）。
2. 失败日志必须包含“缺失能力键名”与“当前候选标识”。
3. `VT-*` 与 Phase0R 真值表行号必须保持一一对应，禁止同一 `VT-*` 复用到语义不同的分支。
4. 进入 Phase3 后，`MT-MATCH-*`/`MT-FALLBACK-*` 为强制输出；`VT-*` 继续用于策略组合验证与回归审计。

实现落地建议：

1. Phase1~Phase5 默认复用 `LogCode_Contract_Template.md`，除阶段特有扩展外不重复定义前缀语义。

---

## 7. 文件组织与命名

本轮文档存放目录：

`doc/ShaderGen_MaterialPresetTable_Redo/`

- `00_MasterPlan.md`（本文件）
- `LogCode_Contract_Template.md`（Phase 通用日志合同模板）
- `Phase0R_VertexPolicy_Decouple.md`
- `Phase0_Billboard_Decommission.md`
- `Phase1_ColorSource_Integration.md`
- `Phase2_SFM_PositionProvider.md`
- `Phase3_MaterialPresetTable_Matcher.md`
- `Phase4_Key_GeometryMode_Cleanup.md`
- `Phase5_Legacy_Runtime_Removal.md`

---

## 8. 分支与提交策略

1. 基线分支：从当前稳定提交拉出执行分支（建议命名 `refactor/shadergen-redo-stable`）。
2. 提交粒度：每阶段至少 1 个可回滚提交，推荐“功能 + 测试”同提交。
3. 保护规则：未通过本阶段验收，不允许进入下一阶段。

---

## 9. 测试与验收总则

### 9.1 阶段门禁（必须同时满足）

1. 编译通过（ShaderGen、SceneGraph、关键测试与示例目标）。
2. 核心示例可运行且无新回归。
3. 本阶段新增测试（或审计测试）通过。
4. 日志可证明本阶段设计已生效。

### 9.2 建议回归样例

1. draw_triangle
2. PBRSpheresECS
3. Text2D 相关示例
4. Gizmo / Terrain / Sky 相关示例
5. 透明与 masked 组合样例（若已有）

---

## 10. 回滚总则

1. 阶段失败优先“整阶段回滚”，而不是局部缝补。
2. 任一阶段出现“示例系统性黑屏/错材质”，立即回退到上阶段 tag。
3. 允许阶段内小步修复，但禁止跨阶段遗留未闭环状态。

---

## 11. 角色分工建议

1. 架构负责人：把关契约、边界、fallback 规则。
2. ShaderGen 实施：实现 Matcher/PresetTable/SFM 解析与接入。
3. 运行时桥接实施：MaterialLibrary/ProgramManager 入口改造。
4. 测试负责人：构建门禁、示例回归、审计日志核验。

---

## 12. 常见误解与纠偏

1. 误解：先删旧表再接 Matcher。
   - 纠偏：必须先接入新主路径，再删除旧表。
2. 误解：RenderPhase 必须第一时间进所有 key。
   - 纠偏：可先在 Matcher 决策使用，待阶段稳定再进 key。
3. 误解：MaterialRecipe 结构拆分是本轮必要前置。
   - 纠偏：本轮以稳定迁移为主，结构重构可后置。

---

## 13. 阶段文档入口

1. Phase0R：`Phase0R_VertexPolicy_Decouple.md`
2. Phase0：`Phase0_Billboard_Decommission.md`
3. Phase1：`Phase1_ColorSource_Integration.md`
4. Phase2：`Phase2_SFM_PositionProvider.md`
5. Phase3：`Phase3_MaterialPresetTable_Matcher.md`
6. Phase4：`Phase4_Key_GeometryMode_Cleanup.md`
7. Phase5：`Phase5_Legacy_Runtime_Removal.md`

---

## 14. 结论

本设计采用“先稳定接入、后删除旧路”的控制策略，目的是减少跨阶段耦合和过渡期双轨失稳。执行中若遵守阶段闭环与门禁，能够在保持示例可运行的前提下完成 ShaderGen 路由体系重建。