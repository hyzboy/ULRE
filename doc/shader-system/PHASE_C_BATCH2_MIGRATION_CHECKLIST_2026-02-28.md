# Phase C Batch-2 迁移清单（VertexLuminance3D / VertexPattleColor3D）

本清单承接 Batch-1（BasicLit / TextureBlinnPhong）收口结果，选择两条低风险、可快速复用模板的 3D 材质作为 Batch-2。

**最后更新**：2026-02-28  
**前置里程碑**：`PHASE_C_BATCH1_MIGRATION_CHECKLIST_2026-02-28.md`（已收口）

---

## 1) 目标与范围

- 目标材质：`VertexLuminance3D`、`VertexPattleColor3D`
- 目标文件：
  - `src/ShaderGen/3d/M_VertexLum3D.cpp`
  - `src/ShaderGen/3d/M_VertexPattleColor3D.cpp`
- 本轮不做：
  - 新光照模型扩展
  - 固定管线白名单运行时接入（属于并行主题）
  - legacy 路径下线

---

## 2) 选型依据（为什么是这两项）

### 2.1 VertexLuminance3D

- 当前仍是 `Std3DMaterial` 旧路径，未接入 `CompileComposedBusinessMaterial(...)`。
- Shader 语义简单：VS 做 `Luminance * MI.Color`，FS 直接输出插值色。
- 与 `VertexColor3D` 模板高度相似，适合低风险快速迁移。

### 2.2 VertexPattleColor3D

- 当前仍是 `Std3DMaterial` 旧路径，未接入 composed/fallback 双轨。
- 主要差异点集中在 `ColorPattle` UBO + `R8UI` 调色板索引输入。
- 可复用 `VertexColor3D` 插值模板骨架，验证“非 mtl 资源命名契约”场景。

---

## 3) Batch-2 公共迁移动作（两材质都执行）

- [ ] 增加 `FixedMaterialDef + ComposedMaterialDef + MaterialLogicDef` 三层定义。
- [ ] 工厂切换为 `CompileComposedBusinessMaterial(...)`，保留 legacy fallback。
- [ ] 提取逻辑头到 `S_<Material>_Logic.h`，避免业务 GLSL 混排在工厂实现内。
- [ ] `required_resources` / descriptor `name` 逐项对齐。
- [ ] 为每个材质新增 1 条模板一致性语义断言测试。
- [ ] 将两条新测试接入 `test/run_shader_system_gate.ps1` 聚焦集。

---

## 4) 材质专属执行清单

## 4.1 VertexLuminance3D

- [ ] 新增 `src/ShaderGen/3d/S_VertexLuminance3D_Logic.h`。
- [ ] VS 语义锚点保留：
  - `MaterialInstance mi = GetMI();`
  - `Output.Color = vi.Luminance * mi.Color`（或等价表达）
- [ ] FS 语义锚点保留：`return Input.Color;`
- [ ] 明确资源依赖：`camera` / `l2w` / `mtl`。

## 4.2 VertexPattleColor3D

- [ ] 新增 `src/ShaderGen/3d/S_VertexPattleColor3D_Logic.h`。
- [ ] VS 语义锚点保留：`Output.Color = color_pattle.color[vi.Color.r]`（或等价索引路径）。
- [ ] FS 语义锚点保留：`return Input.Color;`
- [ ] 明确资源依赖：`camera` / `l2w` / `color_pattle`。
- [ ] 明确输入契约：调色板索引输入仍使用 `VAT_UINT` + `Color`（R8UI 语义）。

---

## 5) 测试与门禁接入

新增测试：
- [ ] `test/VertexLuminance3DTemplateConformanceTest.cpp`
- [ ] `test/VertexPattleColor3DTemplateConformanceTest.cpp`

接入点：
- [ ] `test/CMakeLists.txt`（目标 + add_test + 编号）
- [ ] `test/run_shader_system_gate.ps1`（build target + ctest regex）

测试模式：
- 沿用 Batch-1 语义锚点断言风格（非仅编译通过）。
- 同时检查 `ValidateMaterialLogicDef(...)` 与资源/helper 依赖一致性。

---

## 6) 验收标准（Batch-2 完成定义）

- [ ] 两材质都具备 composed-first + legacy fallback 双轨。
- [ ] 两个模板一致性测试稳定通过并纳入 gate。
- [ ] gate 保持 `PASS`，`composed-diagnostics.jsonl` 链路不退化。
- [ ] 运行日志可区分 composed path 与 fallback path（至少在工厂层打印）。

---

## 7) 推荐执行顺序

1. 先迁移 `VertexLuminance3D`（最接近 `VertexColor3D`，验证流水线模板复用）。
2. 再迁移 `VertexPattleColor3D`（补齐调色板 UBO + uint 输入差异）。
3. 最后一次性接入两条 conformance 测试到 gate 并复验。

---

## 8) 风险与回滚

- 若 `VAT_UINT` / 调色板索引在 composed 生成中出现类型回归，优先保留 legacy fallback 并将问题限制在单材质。
- 若 `color_pattle` 命名与现有 `UBOCommon` 结构名发生偏差，先做命名映射兼容，再做规范收敛。

---

## 9) 变更历史

- 2026-02-28：第一刀完成 `VertexLuminance3D` composed-first 改造（新增 `S_VertexLuminance3D.h` / `S_VertexLuminance3D_Logic.h`、工厂接入 fallback），新增 `test_VertexLuminance3DTemplateConformance` 并纳入 gate；当前 gate 聚焦集为 `11/11` 通过，诊断工件 `count=1`。
- 2026-02-28：创建 Batch-2 执行卡（基于当前源码状态盘点，无功能变更）。
