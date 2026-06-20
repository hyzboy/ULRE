# Phase 3 技术文档：MaterialPresetTable + Matcher 接入运行期主路由

## 1. 阶段目标

将 shader 选择主路径从旧 registry/fallback 迁移到 `MaterialPresetTable + Matcher`，并通过 `MatchedShaderSet` 桥接到运行期材质创建。

这是本轮最高风险阶段，必须满足：

1. 新主路径真实生效（不是旁路实现）。
2. fallback 规则严格按契约执行。
3. 出错时可诊断、可定位、可回滚。

---

## 2. 架构目标图（逻辑）

1. 输入：`MaterialRecipe + GeometrySupply + ResourceSupply + RenderPhase + GlobalConfig`。
2. 候选：从 `MaterialPresetTable` 取当前 preset 对应质量梯度候选。
3. 匹配：`Matcher` 根据 SFM requirement 做集合包含判断。
4. 输出：`MatchedShaderSet`（surface path + composed axis + optional overlays）。
5. 桥接：MaterialLibrary/ProgramManager 使用 MatchedShaderSet 构建最终 create info。

---

## 3. 核心契约

### 3.1 PresetTable 契约

1. 按 `(preset, quality_level, phase)` 查询候选。
2. 质量降级顺序固定：从请求质量向低质量递减。
3. 同一 preset 内可配置多个候选行。

### 3.2 Matcher 契约

1. 先过滤 `supports_phase`。
2. 再检查 `require_va/tex/ubo/ssbo` 是否满足。
3. 不满足则记录失败原因并尝试下一个候选。
4. 全失败则返回 checkerboard fallback 或显式错误。

### 3.3 fallback 禁令

1. 禁止跨 preset fallback。
2. 禁止 silent fallback。
3. 禁止“部分字段来自命中候选、部分字段来自旧 fallback”混合行为。

### 3.4 LogCode 契约（与 MasterPlan/Phase0R 对齐）

1. 本阶段日志规则复用 `LogCode_Contract_Template.md`，本节仅定义 Phase3 增量约束。
2. `MT-MATCH-*`：记录候选筛选、require 不满足、质量降级轨迹。
3. `MT-FALLBACK-*`：记录 fallback 触发与最终 fallback 类型。
4. `VT-OK-*`/`VT-ERR-*`：当输入命中 VertexPolicy 组合验证路径时，必须带出对应 `VT-*` 编号。
5. 一次 `Matcher::Resolve(...)` 调用至少输出一条阶段级 `LogCode`。
6. 失败日志必须包含：候选标识、缺失能力键名、当前 preset、phase、quality。

---

## 4. 接口与数据结构

1. `RenderPhase`：本阶段用于 Matcher 决策。
2. `MaterialPresetTable`：定义 `SurfaceId` 与路径映射。
3. `MatchedShaderSet`：
   - 必填：surface_path、quality_level、render_phase
   - 快照：lighting_model、sky_ambient_model、policy、position_provider
   - 可选：alpha_overlay、transition_overlay、policy_overlay、pass_override
4. `Matcher::Resolve(...)`：返回命中或 fallback 结果。

---

## 5. 建议改动文件

1. 头文件：
   - `inc/hgl/mtl/RenderPhase.h`
   - `inc/hgl/mtl/MaterialPresetTable.h`
   - `inc/hgl/shadergen/Matcher.h`
   - `inc/hgl/shadergen/MatchedShaderSet.h`
   - `inc/hgl/graph/module/ShaderMaterialProgramManager.h`
   - `inc/hgl/mtl/MaterialLibrary.h`
2. 源文件：
   - `src/ShaderGen/MaterialPresetTable.cpp`
   - `src/ShaderGen/Matcher.cpp`
   - `src/ShaderGen/MaterialLibrary.cpp`
   - `src/SceneGraph/module/ShaderMaterialProgramManager.cpp`（按实际路径）
3. 相关路由文件：
   - `src/ShaderGen/RecipeToKey.cpp`（仅接入层，不做阶段4清理）

---

## 6. 执行步骤

1. 定义并填充最小可运行的 PresetTable（覆盖核心 preset）。
2. 实现 Matcher 匹配与失败原因编码。
3. 在 ProgramManager cache miss 路径调用 Matcher。
4. 把 MatchedShaderSet 传入 MaterialLibrary 创建链路。
5. 在 create info 组装处优先使用 `matched.surface_path`。
6. 增加命中/降级/fallback 日志。
7. 运行重点示例回归。

---

## 7. 测试策略

1. 单元测试：
   - PresetTable 查询行为
   - Matcher 对 require 集合判断
   - fallback 规则（禁止跨 preset）
2. 集成测试：
   - RecipeToKeyTest
   - ShaderBuildPipelineSmokeTests
3. 示例回归：
   - draw_triangle
   - PBRSpheresECS
   - Text2D / Gizmo / Terrain / Sky

---

## 8. 验收标准

1. runtime shader 选择日志可证明已经走 Matcher。
2. 不出现跨 preset fallback。
3. 核心示例与测试通过。
4. 回退开关可用（必要时临时回旧路）。
5. 日志前缀符合约定：`MT-MATCH-*`、`MT-FALLBACK-*`、`VT-*`。
6. 若命中 Phase0R 组合验证路径，`VT-*` 必须与真值表行语义一一对应。

---

## 9. 风险与防错

1. 风险：Matcher 接入但旧路径仍覆盖结果。
   - 防错：在关键点打印“最终采用路径来源”。
2. 风险：PresetTable 不完整导致大量 fallback。
   - 防错：先覆盖核心 preset，再逐步扩展；fallback 统计纳入门禁。
3. 风险：surface_path 覆盖不全导致 compose 异常。
   - 防错：MaterialLibrary 桥接层做路径有效性断言与诊断。

---

## 10. 回滚方案

1. 保留“旧主路由开关”，出现大面积回归时立即切回。
2. 回滚顺序：先回 ProgramManager 接入提交，再回 Matcher 核心实现。

---

## 11. 阶段完成定义（DoD）

1. Matcher 成为运行期主路由。
2. MatchedShaderSet 在材质创建链生效。
3. fallback 契约被测试和日志双重证明。
4. 允许进入 Phase 4。