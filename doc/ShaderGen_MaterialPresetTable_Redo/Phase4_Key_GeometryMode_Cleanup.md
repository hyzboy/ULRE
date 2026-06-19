# Phase 4 技术文档：MaterialVariantKey 精简与 GeometryMode 清理

## 1. 阶段目标

在 Matcher 主路由稳定后，清理旧 key 中的高风险轴，去除 `GeometryMode` 对运行期路由的影响，收敛 key 语义，降低错配概率。

本阶段原则：

1. 先确认 Phase 3 稳定，再做结构性清理。
2. 一次只改一类轴，避免多轴联动引发定位困难。
3. key 变更必须伴随 toolchain/version 变更。

---

## 2. 范围定义

### 2.1 纳入

1. 从 `MaterialVariantKey` 删除 `geometry_mode`。
2. 视方案成熟度引入 `render_phase` 到 key（推荐本阶段后半执行）。
3. `MaterialVariantRow` 退化为快照/诊断结构。
4. `RecipeToKey` 逻辑收敛，去除旧 demand/fallback 依赖。
5. 更新 hash/equals/registry hash 契约。

### 2.2 不纳入

1. MaterialRecipe 大规模三段式重构。
2. Overlay/PassResolver 的完整注入落地。

---

## 3. 关键设计决策

1. 路由几何语义优先由 `vertex_transform_policy + position_provider` 表达。
2. key 只保留对缓存命中和渲染语义必要的轴。
3. `render_phase` 进入 key 后，必须触发缓存版本升级，避免旧缓存污染。

---

## 4. 建议改动文件

1. 头文件：
   - `inc/hgl/mtl/MaterialVariantKey.h`
   - `inc/hgl/mtl/MaterialVariantRow.h`
   - `inc/hgl/mtl/MaterialKeyToolchainVersion.h`
   - `inc/hgl/mtl/RecipeToKey.h`
2. 源文件：
   - `src/ShaderGen/MaterialKey.cpp`
   - `src/ShaderGen/ShaderProgramKey.cpp`
   - `src/ShaderGen/RecipeToKey.cpp`
   - `src/ShaderGen/VariantRegistry.cpp`（仅保留诊断必要部分）
3. 测试：
   - `src/ShaderGen/tests/MaterialKeyTest.cpp`
   - `src/ShaderGen/tests/MaterialKeyAbiSnapshot.cpp`
   - `src/ShaderGen/tests/RecipeToKeyTests.cpp`

---

## 5. 执行步骤

1. 移除 key 中 `geometry_mode` 字段与相关 hash 参与项。
2. 更新序列化/快照测试基线。
3. 清理 `RecipeToKey` 中对旧 demand/fallback 调用。
4. 评估并接入 `render_phase` 到 key。
5. 更新 toolchain/version，强制缓存重建。
6. 执行回归测试与示例验证。

---

## 6. 验收标准

1. key 相关单元测试与 ABI 快照测试通过。
2. 缓存重建后行为稳定，无旧缓存误命中。
3. 关键示例渲染无新增错误。
4. 运行日志可追踪 key 组成轴。

---

## 7. 风险与防错

1. 风险：删除 geometry_mode 影响旧路径依赖。
   - 防错：先确认 Matcher 路由已覆盖关键路径，再删字段。
2. 风险：key 变更未触发版本升级导致脏缓存。
   - 防错：强制 bump `MaterialKeyToolchainVersion`。
3. 风险：Row 退化过早影响调试能力。
   - 防错：保留必要 snapshot 字段与 Dump 输出。

---

## 8. 回滚方案

1. 若出现 key 级别系统性错材质，整体回滚 Phase 4。
2. 保留 Phase 3 成果，禁止“局部恢复 geometry_mode”混合状态。

---

## 9. 阶段完成定义（DoD）

1. key 语义收敛完成，`geometry_mode` 不再是 runtime 路由轴。
2. `render_phase`（若纳入）已稳定进入 key 且缓存版本同步更新。
3. 允许进入 Phase 5 进行旧路径最终退场。