# Phase 5 技术文档：旧 runtime 路由退场与最终收尾

## 1. 阶段目标

在前四阶段稳定后，完成旧 runtime 路由表的退场，形成单一路由架构，减少长期维护负担。

核心目标：

1. `kBuiltinVariants[]` 不再参与 runtime shader 选择。
2. `PresetDemandTable` 退出 runtime 路径。
3. 旧 fallback 链路移除或降级为诊断用途。
4. 架构收口到 `PresetTable + Matcher`。

---

## 2. 前置条件（必须满足）

1. Phase 3 的 Matcher 主路由已稳定。
2. Phase 4 的 key 清理已完成且通过回归。
3. 关键 preset 已有完整 PresetTable 配置与 SFM 注解支持。

若任一前置条件不满足，不得执行本阶段删除动作。

---

## 3. 变更范围

### 3.1 纳入

1. 删除或停用 `VariantRegistry.cpp` 中 `kBuiltinVariants[]` runtime 依赖。
2. 删除 `PresetDemandTable.h/.cpp` 或改为历史兼容占位（不参与主流程）。
3. 清理 `RecipeToKey` 里的旧 fallback 调用路径。
4. 删除确认无引用的过时适配文件（如 `BuiltinVariantEntry` 仅历史用途时）。
5. 更新 CMake 列表与测试用例。

### 3.2 不纳入

1. 新功能扩展（overlay/pass resolver 完整注入）。
2. 额外架构试验性重构。

---

## 4. 建议改动文件

1. 头文件：
   - `inc/hgl/mtl/MaterialVariantRegistry.h`
   - `inc/hgl/mtl/PresetDemandTable.h`（若存在）
   - `src/ShaderGen/BuiltinVariantEntry.h`（按引用决定）
2. 源文件：
   - `src/ShaderGen/VariantRegistry.cpp`
   - `src/ShaderGen/PresetDemandTable.cpp`（若存在）
   - `src/ShaderGen/RecipeToKey.cpp`
   - `src/ShaderGen/CMakeLists.txt`
3. 测试：
   - 迁移依赖旧表的测试到新路径语义

---

## 5. 执行步骤

1. 对 runtime 调用链做最终追踪，确认旧表仍在何处被调用。
2. 先改调用链到新路径，再删旧表定义。
3. 移除旧路径相关构建项与 include。
4. 修复受影响测试并补充“禁止跨 preset fallback”回归项。
5. 全量回归并生成阶段报告。

---

## 6. 验收标准

1. 运行期路由唯一来源为 PresetTable + Matcher。
2. 代码中不存在旧表参与 runtime 决策的调用点。
3. 所有关键示例可运行，核心测试通过。
4. 文档与日志可清楚解释最终路由路径。

---

## 7. 风险与防错

1. 风险：删旧表后遗漏隐式依赖（如统计/诊断模块）。
   - 防错：先将旧表改为“诊断只读”并观察，再彻底删除。
2. 风险：旧测试强耦合旧结构，短期大量失败。
   - 防错：先迁移测试语义，再删实现。
3. 风险：删得过快导致定位困难。
   - 防错：分提交删减，每步可单独回滚。

---

## 8. 回滚方案

1. 以阶段 tag 为锚点进行整阶段回滚。
2. 若仅删除动作导致问题，优先恢复被删文件并禁用其 runtime 入口。

---

## 9. 阶段完成定义（DoD）

1. 旧 runtime 路由链完全退出。
2. 工程进入“单一路由、可维护、可扩展”稳态。
3. 本轮重做目标达成，可进入后续优化迭代（非本轮）。