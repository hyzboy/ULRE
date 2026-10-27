# 时间窗封存目录 (doc/archive)

本目录存放**按 git 时间窗封存**的历史档：以 `2026-10-28` 为基准，**7 天窗口内（HEAD 提交日 ≥ 2026-10-21）的活跃档保留在 `doc/`，其余全部移入本目录**。
封存 = 内容原样保留、路径可追溯（`git mv`），但**不再作为现行依据维护**；引用若干处已同步改写为 `doc/archive/...`。

## 与 `doc/deprecated/` 的区别

| 目录 | 判据 | 含义 |
|---|---|---|
| `doc/deprecated/` | **内容性作废**（档首自述作废 / 描述对象已从代码整体消失 / 计划全无实施痕迹） | 读了会误导，**严禁**作为实现依据 |
| `doc/archive/` | **时间性过期**（仅因 7 天未更新） | 内容未必错，但已不在活跃维护面；**可能仍准确**，引用前需对照代码 |

> 换句话说：`deprecated` 是「判定失效」，`archive` 是「停止维护」。两者都不可当作最新口径。

## 封存清单（20 份，按原提交日升序）

| 文档 | 最后提交日 | 原位置 |
|---|---|---|
| `mipmap-level-test.md` | 2026-09-13 | `doc/` |
| `async-texture-upload-and-bindless.md` | 2026-09-24 | `doc/` |
| `csm-readback-and-leak-followup.md` | 2026-10-01 | `doc/` |
| `baseline/README.md` | 2026-10-01 | `doc/baseline/` |
| `shadow-ubo-inflight-overwrite.md` | 2026-10-07 | `doc/` |
| `instance-batching-and-static-bake-design.md` | 2026-10-08 | `doc/` |
| `02-entity-component-list-copies.md` | 2026-10-13 | `doc/analysis/ecs-audit-2026-10/` |
| `07-material-four-key-spaces.md` | 2026-10-13 | `doc/analysis/ecs-audit-2026-10/` |
| `08-materialruntime-row-key-duplication.md` | 2026-10-13 | `doc/analysis/ecs-audit-2026-10/` |
| `alpha-test-shadow-masked-caster-fix-chain-2026-09-26.md` | 2026-10-15 | `doc/` |
| `global-addresses-bda-unification-plan.md` | 2026-10-15 | `doc/` |
| `shadow-component-and-automated-pipeline-design.md` | 2026-10-15 | `doc/` |
| `01-component-mask-cache-view.md` | 2026-10-15 | `doc/analysis/ecs-audit-2026-10/` |
| `primitive-geometry-vdm.md` | 2026-10-15 | `doc/ecs/` |
| `transform-data-management-and-transfer.md` | 2026-10-15 | `doc/ecs/` |
| `future/e2-gather-run-length-merge-plan.md` | 2026-10-15 | `doc/future/` |
| `future/ULRE_Architecture_Phase3_Entity64B_Binary_Scene_BlitLoading.md` | 2026-10-15 | `doc/future/` |
| `future/ULRE_Architecture_Technical_Review_20260930.md` | 2026-10-15 | `doc/future/` |
| `03-renderitem-descriptor-two-tables.md` | 2026-10-16 | `doc/analysis/ecs-audit-2026-10/` |
| `future/ULRE_Transform_System_Completion_Design_and_Plan.md` | 2026-10-18 | `doc/future/` |

## 时间窗规则的例外（保留在 `doc/`）

以下 6 份同样超出 7 天窗口，但因**被代码注释引用**而豁免，避免打断源码内的指引：

| 文档 | 最后提交日 | 代码引用数 |
|---|---|---|
| `world-addresses-and-camera-model-plan.md` | 2026-10-15 | 14 |
| `future/ULRE_FINAL_TARGET_v2_设计约束.md` | 2026-10-18 | 3 |
| `build-and-module-structure-audit-2026-10.md` | 2026-10-18 | 2 |
| `future/ULRE_Phase0-1_Reality_Audit_and_Task_Plan.md` | 2026-10-18 | 1 |
| `future/stage_B_execution_plan.md` | 2026-10-16 | 1 |
| `future/entitygpu-and-ulrescene-design.md` | 2026-10-16 | 1 |

> `doc/deprecated/` 已在此前单独封存过，不再套用时间窗规则。`gpu-cull-multipass-design-and-shadow-hang-analysis.md` 已由手工操作移入 `doc/deprecated/`，故不在本表。
