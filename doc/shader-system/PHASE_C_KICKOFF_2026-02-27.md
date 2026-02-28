# Phase C 启动页（2026-02-27）

> 本页是 **执行入口**，不是新的总体计划。  
> 唯一路线图与状态源仍为：[../../SHADER_SYSTEM_REFACTOR_PLAN.md](../../SHADER_SYSTEM_REFACTOR_PLAN.md)

---

## 1) 启动目标（本轮）

- 在现有稳定基线上推进 Phase C 的“批量迁移可维护化”，不破坏现有渲染可用性。
- 保持 Composed-first + legacy fallback 双轨，优先补“验证闭环”而不是扩展新特性。

---

## 2) 范围与边界

### In Scope

- 为已迁移材质补强可验证闭环（桥接校验 + 运行回归 + 门禁可追踪）。
- 统一文档口径，避免“状态在多个文档里冲突”。

### Out of Scope

- 不新增 Pipeline 模式矩阵能力（留给 Phase D）。
- 不执行 legacy 路径下线（留给 Phase E）。

---

## 3) 本轮门禁

- `test/run_shader_system_gate.ps1` 必须通过（构建 + 关键测试执行）。
- `06b_BasicLitMeshesECS` 与 `06c_TextureBlinnPhongMeshesECS` 可运行。
- 新增改动不得破坏 fallback 行为。

---

## 4) 首批任务（按顺序）

1. ✅ 补“全量桥接校验”最小可运行集合（PureColor / VertexColor / Gizmo）。
2. ✅ 把桥接校验纳入可重复执行脚本（并接入现有 gate 或并行 gate）。
3. ✅ 为每个已迁移材质补 1 条最小回归断言（编译或运行级）。
4. ✅ 更新 `PHASE_B_COMPLETION_SUMMARY.md` 与本页状态，保持单一口径。

> 进度：任务 1、2、3、4 已落地（`test_BridgeValidation3Materials` 已包含 3 个材质的语义断言并接入 gate，文档状态已同步）。

> 增量：已完成 Helper 注入冲突检测最小版（业务代码自定义 helper 时，跳过 builtin 重复注入并输出冲突诊断），并通过 `test_HelperInjectionConflict` 覆盖。

> 增量（增强）：已接入结构化冲突标记 `// HELPER_CONFLICT: ...`，并支持 strict 模式（`ULRE_HELPER_CONFLICT_STRICT=1` 注入 `#error ULRE_HELPER_CONFLICT`）；新增 `test_HelperInjectionConflictMatrix` 覆盖 VS/FS 冲突矩阵与 strict 行为。

> 增量（闭环）：已新增 `test_ComposedDiagnosticsAggregation` 并接入 gate；`test/run_shader_system_gate.ps1` 现会额外执行该测试的 verbose 采集并产出 `gate-artifacts/composed-diagnostics.jsonl`，当前稳定为 `count>=1`。

---

## 5) 完成定义（DoD）

- 三个逻辑样例可稳定通过桥接校验。
- gate 日志能明确区分“编译失败 / 校验失败 / 运行失败”。
- gate 能稳定导出可机读诊断：`composed-diagnostics.jsonl` 至少包含 1 条 `[ComposedBusiness][Diagnostics]` JSON 行。
- 文档状态无自相矛盾项。

---

## 6) 快速执行命令

- 构建示例：`cmake --build build --config Debug --target 06b_BasicLitMeshesECS 06c_TextureBlinnPhongMeshesECS -j 8`
- 运行 gate：`./test/run_shader_system_gate.ps1`

---

## 6.1) 首批迁移模板入口

- 模板文档：[`PHASE_C_MATERIAL_MIGRATION_TEMPLATE_V1.md`](PHASE_C_MATERIAL_MIGRATION_TEMPLATE_V1.md)
- 使用范围：PureColor / VertexColor / Gizmo 三类首批模板与共性检查清单
- Batch-1 执行卡：[`PHASE_C_BATCH1_MIGRATION_CHECKLIST_2026-02-28.md`](PHASE_C_BATCH1_MIGRATION_CHECKLIST_2026-02-28.md)

---

## 7) 当前状态

- 状态：✅ 本轮首批任务已完成
- 状态：✅ 本轮首批任务与诊断工件闭环已完成
- 负责人：Shader System 维护团队
- 最近更新：2026-02-28
