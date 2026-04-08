# VIL 与材质资产彻底解耦实施总计划

## 1. 目标

彻底移除 `MaterialAssetRecord` 中的 `mi_vil_overrides` 语义，让 VIL 在运行时仅由 `Geometry` 的真实 VAB 信息决定；保留“无几何时回退 default VIL”作为唯一兼容兜底，并确保同一语义材质可稳定支持多 Geometry、多 VIL 与多渲染通道。

## 2. 当前状态（截至 2026-04-08）

- [x] `MaterialAssetRecord` 中 `VILOverride` / `mi_vil_overrides` 已删除。
- [x] `MaterialAssetRegistry` 运行时不再读取 record 侧 overrides，改为 geometry-first + default fallback。
- [x] example 侧 overrides 初始化块已清理。
- [x] 全仓检索 `mi_vil_overrides|VILOverride` 为 0。
- [x] 全量构建 `ALL_BUILD` 已通过。
- [x] deferred/no-geometry 场景诊断与升级告警已完成（`MaterialAssetRegistry` 4 个 atomic fallback 计数器 + pow2 节流告警；`RenderPrimitiveCollectSystem` deferred 无 geometry 告警）。
- [x] `GeometrySignature` 强布局签名（VAB format/stride hash）已完成：新增 `geometry_layout_hash` 字段，纳入 `operator==` 与 `VariantKeyHash`，在 `RenderPrimitiveCollectSystem` 的 deferred 路径中通过 `ComputeGeometryLayoutHash()` 填充。
- [x] Phase 3 全部诊断字段已完成（semantic_id/entity_id/vil_hash/geometry_layout_hash/fallback_count 纳入 ResolveMI 日志；PrimitiveBatchPipeline 增加 layout-diversity 汇总与 fallback 统计）。
- [ ] shadow/early-z 子集属性策略尚未正式收敛。
- [x] shadow/early-z 子集属性策略已完成（Option B：shadow/earlyz VS 复用 forward VS，VIL 跨 pass 一致，无 VAB/VIF mismatch 风险）。
- [ ] legacy 入口收口与文档化未完成。

## 3. 范围边界

### 纳入范围

- runtime 主链：`MaterialAssetRegistry` / `MaterialManager` / `Primitive` / ECS render collect & batch path。
- `example/**` 全量调用面。

### 排除范围

- 离线资产转换工具的历史格式兼容（若存在，迁移到独立 DTO，不回流 runtime record）。

## 4. 分阶段计划

## Phase 0: 规则冻结与入口约束

1. 固化约束：新增代码禁止任何 record 侧 VIL 覆写写入。
2. 统一约定：`ResolveVIL` 只允许 geometry-first + default fallback。
3. 注释/日志同步到关键入口，避免回归。

交付标准：规则可检索、可审查、可被日志验证。

## Phase 1: 核心删除（已完成）

1. 删除 `MaterialAssetRecord::VILOverride` 和 `mi_vil_overrides`。
2. 删除 `MaterialAssetRegistry` 中 overrides 分支逻辑。
3. 更新接口注释与语义哈希说明。

交付标准：编译通过；`mi_vil_overrides|VILOverride` 全仓零引用。

## Phase 2: 调用面收敛与防回归

1. 清理 `src/**` 中所有 residual override 语义（包括 legacy 兼容入口语义）。
2. 对 deferred primitive 增加约束：首次 `ResolveMI` 缺 geometry 时高优先级告警 + fallback。
3. 若连续多帧缺 geometry，升级计数并打点，便于定位错误流程。

交付标准：关键路径日志可定位；无 silent fallback 风险。

## Phase 3: 运行时一致性增强

1. ✅ 在 `GeometrySignature` 增加更强布局签名（VAB format/stride hash）。
2. ✅ 在 `RenderPrimitiveCollectSystem` / `PrimitiveBatchPipeline` 增加诊断字段：
   - `semantic_id` ✅（ResolveMI log）
   - `entity_id` ✅（ResolveMI log）
   - `vil_hash` ✅（ResolveMI log + PrimitiveBatchPipeline failure log）
   - `geometry_layout_hash` ✅（ResolveMI log）
   - `fallback_count` ✅（`frame_vil_from_default` per-frame in PrimitiveBatchPipeline; slot.vil==null at ResolveMI）
3. ✅ 验证"同语义材质 + 不同几何布局"不会错误复用（layout-diversity per-frame summary in PrimitiveBatchPipeline）。

交付标准：缓存分化正确，无错误 pipeline/VIL 复用。

## Phase 4: 子集属性渲染策略（shadow/early-z）

**已完成（Option B 已落地）。**

Option A（进阶，暂缓）：在 `RuntimeMaterialRequest` 引入 `pass_type`，按 pass 做 geometry 属性子集映射。

Option B（已实现）：`GetCompositorVSPath` 中 `ShadowOpaque`/`ShadowMasked`/`EarlyZSolid`/`EarlyZMasked` 全部返回与 `ForwardOpaque` 相同的 VS 路径（Lit→`main_forward_opaque.vert.glsl`；Unlit→`main_forward_unlit.vert.glsl` via default）。shadow/earlyz pipeline 与 forward pipeline 共享 VIL，VAB/VIF mismatch 风险被根除。Fragment shader stub（后续实现）不影响 VIL 兼容性。

✅ 交付标准满足：shadow/early-z VS 与 forward VS 共享 VIL，无 VAB/VIF mismatch 风险，ULRE.ShaderGen 构建通过。

## Phase 5: 兼容窗口收口

1. `AcquireMI/CreateMI` 保留 deprecated，但彻底去除旧 overrides 语义遗留描述。
2. 更新迁移文档与示例注释，明确新范式。
3. 输出最终改造报告与回归清单。

交付标准：legacy 入口职责单一，迁移路径清晰。

## 5. 关键文件清单

- `inc/hgl/mtl/MaterialAssetRecord.h`
- `inc/hgl/graph/module/MaterialAssetRegistry.h`
- `src/SceneGraph/module/MaterialAssetRegistry.cpp`
- `inc/hgl/graph/module/RuntimeMaterialRequest.h`
- `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp`
- `src/ecs/support/PrimitiveBatchPipeline.cpp`
- `src/SceneGraph/mesh/Primitive.cpp`
- `src/SceneGraph/module/MaterialManager.cpp`
- `example/**`

## 6. 验证与验收

1. 检索验收：
   - `mi_vil_overrides`
   - `VILOverride`
   结果均为 0（离线兼容目录若存在需单独白名单）。

2. 构建验收：
   - `MSBuild e:/ULRE/build/ULRE.sln /p:Configuration=Debug /p:Platform=x64 /m:4`
   - 或 `cmake --build e:/ULRE/build --config Debug --target ALL_BUILD -j 6`

3. 示例冒烟：
   - `example/Gizmo/RayPicking.cpp`
   - `example/Gizmo/GizmoUsageExample.cpp`
   - `example/Gizmo/PlaneGrid3D.cpp`
   - `example/Environment/DomeSkyMinimal.cpp`
   - `example/Texture/texture_rect_array.cpp`
   - `example/Basic/BillboardIconECS/*`

4. 日志验收：
   - 无新增 `VAB/VIF mismatch`
   - fallback 仅在预期无几何场景出现
   - no-geometry fallback 不持续增长

5. 缓存一致性：
   - 同 semantic material + 不同 geometry layout 时，pipeline/VIL 正确分化，不错误复用。

## 7. 风险与缓解

1. 风险：批量脚本清理 initializer 可能破坏 C++ 聚合语法。
   - 缓解：批后做正则体检 + 问题面板检查 + 全量编译。

2. 风险：deferred 路径在 geometry 未就绪时 fallback 过多导致隐藏问题。
   - 缓解：加分级告警与累计计数阈值。

3. 风险：shadow 子集属性策略未统一，导致布局期望不一致。
   - 缓解：先采用 Option B（稳定优先），再评估 Option A。

## 8. 执行建议

- 以“可回滚小批次”推进：
  1) 规则与诊断
  2) 签名增强
  3) shadow 策略
  4) 兼容收口
- 每批次必须附带：检索截图/日志摘要 + 全量构建结果 + 最小冒烟记录。
