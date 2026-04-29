# Sprite2D 迁移 — Step 8：收尾

> 状态：仅文档与 commit 整理；不涉及代码逻辑变化
> 风险等级：极低
> 预计耗时：2–3 小时

---

## 0. 目标

1. 把所有架构 / 设计文档里的 "Billboard" / "Quad" 术语统一改写为 "Sprite2D"。
2. 写完整的 CHANGELOG / commit message / PR 描述。
3. 移除 `Sprite2DComponent::DeserializeFromRecord` 中的 legacy `"Quad"` 兼容分支（视情况可推迟到下一个 release）。
4. 在分支上打里程碑 tag。

---

## 1. 前置条件

- Step 7 通关：旧符号全部删除、所有示例视觉零退化、序列化兼容验证完成。
- `feature/sprite2d` 分支可干净 merge 回主分支（无冲突）。

---

## 2. 涉及文件

| 路径 | 改动 |
|---|---|
| `doc/PipelineMaterialBatch_架构拆分说明.md` | Billboard → Sprite2D 术语统一 |
| `doc/ECS_响应式参与系统设计.md` | 同上 |
| `README.md` | 加一段"breaking change"说明 |
| `CHANGELOG.md` | 写本次迁移的完整条目 |
| 任何提到 `QuadComponent` / `BillboardRenderPipelineGroup` 的过期文档 | 改写或标记为 "Historical / pre-Sprite2D" |
| `src/ecs/components/Sprite2DComponent.cpp` | （可选）移除 legacy `"Quad"` deserialize 分支 |

---

## 3. 执行步骤

### 3.1 文档术语统一

跑一次 grep 找出所有还含旧术语的 .md：

```pwsh
git grep -in -E "Billboard|QuadComponent|BillboardRenderPipelineGroup|CreateBillboard2D" -- "doc/*.md" "*.md"
```

逐文件人工修订：

- 概念性描述：Billboard → Sprite2D
- API 名字：`QuadComponent` → `Sprite2DComponent`、`BillboardRenderPipelineGroup` → `Sprite2DRenderPipelineGroup`
- 流水线图 / 表格：替换枚举名

> Step 1–8 的迁移文档（`doc/Sprite2D_Step*.md`）**保留 Billboard 字样**，因为它们记录的是迁移历史，本身就需要这些词。

### 3.2 CHANGELOG 条目

`CHANGELOG.md` 顶部新增：

```markdown
## [Unreleased] — Sprite2D refactor

### Breaking
- Removed `QuadComponent`, `BillboardRenderPipelineGroup`, `CreateBillboard2DFixed/Dynamic`,
  `BillboardSizeUVec2` schema, and `GeometryMode::BillboardCameraFacing/AxisLocked` enum values.
- All sprite/billboard rendering now uses unified `Sprite2DComponent` +
  `Sprite2DRenderPipelineGroup` driving a built-in unit-square mesh.
- Per-instance schema changed: 16B `BillboardSizeUVec2` → 32B `Sprite2DTransform`
  (size + pivot + rotation + tint + flags). External materials authored against the
  old schema will not link.

### Added
- `Sprite2DComponent` with `pivot`, `rotation`, `tint` controls.
- `Sprite2DCameraFacing` / `Sprite2DAxisLocked` material variants.
- Example: `03_Sprite2DPerspectiveECS` demonstrating all 7 capabilities.

### Migration
- For pre-existing scene files using `"Quad"` records, `Sprite2DComponent::DeserializeFromRecord`
  reads them with default pivot=(0.5,0.5), rotation=0, tint=white. This legacy bridge will
  be removed in the next release — re-save scenes once after upgrading.

### Internal
- See `doc/Sprite2D_迁移方案.md` and `doc/Sprite2D_Step1..Step8.md` for the full
  migration history.
```

### 3.3 README breaking change

`README.md` 在 features 节附近加一行简洁提示：

```markdown
> ⚠️ **Breaking change** as of `<commit-date>`: `QuadComponent` and Billboard pipeline group have
> been removed in favor of unified `Sprite2DComponent`. See `CHANGELOG.md`.
```

### 3.4 （可选）移除 legacy `"Quad"` deserializer

如果团队同意"保留一个 release 周期再清"，则本步**先不动**；下个 release tag 后再删。

如果决定本次清掉：

`src/ecs/components/Sprite2DComponent.cpp`：

```diff
 void Sprite2DComponent::DeserializeFromRecord(const ComponentRecord& record, ...)
 {
-    if (record.type == "Quad") {
-        // ... legacy bridge ...
-        return;
-    }
     // 正常 Sprite2D 路径
 }
```

> 若删，`CHANGELOG.md` 同步更新："Removed legacy 'Quad' record deserializer"。

### 3.5 打 tag

```pwsh
git tag -a sprite2d-migration-done -m "Sprite2D unified refactor complete"
git push --tags
```

### 3.6 最终 PR / commit message 模板

```
Refactor: drop Billboard/Quad, unify under Sprite2D (unit-square mesh + pivot/size)

This PR completes the multi-step migration documented in
doc/Sprite2D_迁移方案.md and doc/Sprite2D_Step1..Step8.md.

Highlights:
- Single Sprite2DComponent replaces QuadComponent + Billboard variants.
- Per-vertex transform on a built-in 4-vertex unit-square mesh; no more
  shader-side 1-point expansion.
- New per-instance Sprite2DTransform schema adds pivot, rotation, tint.
- 03_Sprite2DPerspectiveECS demonstrates all 7 capability scenarios.
- Drawcall count unchanged (1 per domain via Texture2DArray batching).
- Legacy "Quad" scene records deserialize with default pivot/rotation/tint;
  bridge will be removed next release.

Breaking: BillboardRenderPipelineGroup, QuadComponent, CreateBillboard2D*,
BillboardSizeUVec2, GeometryMode::Billboard* removed.
```

---

## 4. 验证

- [ ] `git grep -in -E "Billboard|QuadComponent" -- "doc/*.md" "*.md"`：除 `Sprite2D_Step*.md` 与 `CHANGELOG.md` 外 0 命中。
- [ ] CHANGELOG / README 写明 breaking change。
- [ ] tag 已推送。
- [ ] PR 链接 / commit hash 归档到 `doc/Sprite2D_迁移方案.md` 末尾的"完成情况"小节（手动加）。

---

## 5. 回滚方案

文档改动可单独 revert，不影响代码：

```pwsh
git revert <doc-commit-hash>
```

如要整体回滚到迁移开始前：

```pwsh
git reset --hard <pre-sprite2d-baseline-tag>
```

---

## 6. Step 8 通关条件

- [ ] 所有架构 / 设计文档术语统一。
- [ ] CHANGELOG / README 完成。
- [ ] tag `sprite2d-migration-done` 推送。
- [ ] `doc/Sprite2D_迁移方案.md` 末尾"完成情况"小节填写日期 + PR 链接。
- [ ] 本 .md 末尾追加 `已通过：YYYY-MM-DD by xxx`。

---

## 7. 后续维护建议

1. **下个 release 删 legacy `"Quad"` deserializer**：在下一次发版前 grep 一次，确认没有内部资产依赖即可删除。
2. **扩展 mesh 表**：方案 §6 提到 `shared_unit_square_primitive` 可升级为 `unordered_map<mesh_id, Primitive*>`。当出现实际多 mesh 需求（glyph、九宫格、任意多边形）时再做，shader / per-MI schema 不需改。
3. **性能监控**：Sprite2D 的 per-vertex transform 比旧 1-vertex shader 多 3 个顶点的开销；对大量 sprite 场景（>10k）做一次 profiler 对比，确认整体吞吐没退化。
