# MaterialInstance 句柄化重构计划（Domain 索引化）

## 1. 目标与原则

### 1.1 最终目标

将 `MaterialInstance` 从“渲染绑定容器”收缩为“实例数据句柄”，最终仅保留以下职责：

- `domain_id`（或可平滑过渡为 `MaterialResourceDomain*` -> `domain_id`）
- `mi_id`
- `mit_slot_offset[]`
- `mit_packed_count`
- `mit_packed`

`MaterialTemplate`、`VIL`、`GraphicsPipelinePreset` 不再属于 `MaterialInstance`。

### 1.2 核心设计原则

- 实例数据定位应只依赖域与索引，不依赖材质模板。
- 渲染选择（材质模板、VIL、preset）应由渲染路径（例如 `PrimitiveMaterialSlot`）承载。
- 支持未来 `domain` 整体替换：句柄稳定，底层域可替换。

---

## 2. 目标架构

### 2.1 句柄结构（建议终态）

```cpp
struct MaterialInstanceHandle
{
    uint32_t domain_id = 0;
    int32_t  mi_id = -1;
    uint32_t domain_generation = 0; // 强烈建议
};
```

说明：

- `domain_id + domain_generation` 用于防止域被替换/复用后的悬挂访问。
- 访问 `MIData` 时通过 manager 的域表间接查找真实 `MaterialResourceDomain`。

### 2.2 职责归位

- `MaterialInstance`（或其替代句柄）
  - 只负责实例数据地址定位与 MIT 数据读写。
- `PrimitiveMaterialSlot`
  - 承载渲染绑定信息：`material_template + vil + preset + domain + mi_id`。
- `MaterialManager/Registry`
  - 负责域生命周期、索引分配、domain->material 绑定策略。

---

## 3. 分阶段执行计划

## Phase A：接口铺垫（无行为变化）

目标：先引入新接口，保留旧路径，确保可渐进迁移。

工作项：

1. 在 `MaterialManager` 新增“按 domain 分配实例槽并返回 slot/handle”的接口。
2. 在 `MaterialAssetRegistry` 新增 slot-first API（返回 `PrimitiveMaterialSlot`）。
3. 保留 `AcquireMaterialInstance` 兼容入口，但标记 deprecated。

验收标准：

- 新接口可并行使用。
- 旧调用点不受影响，工程可编译。

---

## Phase B：调用方去耦（先 ECS 热路径）

目标：所有高频渲染路径不再依赖 `MaterialInstance::GetMaterial/GetVIL/GetRenderPreset`。

优先改造对象：

1. `TextRenderPipeline`
2. `QuadResourcePrepareSystem`
3. `QuadMaterialBindingSystem`

改造策略：

- 用 `PrimitiveMaterialSlot` 直接传入 `CreatePrimitive/BindMaterialSlot`。
- `VIL` 从 `MaterialTemplate` 或 `VILConfig` 直接生成，不再从 `MaterialInstance` 取。
- `preset` 放入 request/slot，不通过 `MaterialInstance` 存取。

验收标准：

- 上述模块不再调用 `GetMaterial/GetVIL/GetRenderPreset`。
- 行为与当前一致（纹理绑定、MIT 层索引、draw 输出一致）。

---

## Phase C：Primitive 路径统一为 Slot

目标：`Primitive` 不再依赖 MI 作为构造输入。

工作项：

1. 统一采用 `DirectCreatePrimitive(Geometry*, const PrimitiveMaterialSlot&)`。
2. 删除或降级 `DirectCreatePrimitive(Geometry*, MaterialInstance*, ...)` 为兼容薄壳。
3. `Primitive` 内字段仍保留 `material_template/domain/mi_id/vil/preset`（这是渲染绑定层，不是 MI 职责）。

验收标准：

- 所有业务路径通过 slot 构建/更新 primitive。
- MI 路径仅剩少量兼容调用（可统计归零）。

---

## Phase D：收缩 MaterialInstance 到最小职责

目标：MaterialInstance 不再包含渲染绑定语义。

字段删除目标：

- `material_manager`
- `material`
- `vil`
- `render_preset`

方法删除目标：

- `GetMaterial()`
- `GetVIL()`
- `GetRenderPreset()` / `SetRenderPreset()`

保留方法：

- `GetMIData()`
- `WriteMIData(...)`
- `InitMITLayout(...)`
- `SetTextureArrayLayer(...)`
- `GetTextureArrayLayer(...)`

验收标准：

- MI 头/源文件只剩 domain + mi_id + mit 系列逻辑。
- 全仓库无对已删除 getter 的引用。

---

## Phase E：domain 指针改为 domain_id（支持域整体替换）

目标：完成你提出的“domain 存实质数据，实例只存索引”的终态。

工作项：

1. 在 `MaterialManager` 建立 `domain_table[domain_id]` 与 generation。
2. MI 从 `domain*` 迁移为 `domain_id + generation + mi_id`。
3. 所有 `GetMIData/WriteMIData` 通过 `ResolveDomain(domain_id, generation)` 间接访问。
4. 提供 `ReplaceDomain(domain_id, new_domain, new_generation)` 支持热替换。

验收标准：

- `MaterialInstance` 不再持有裸 `domain*`。
- domain 替换后句柄可检测过期，不发生野指针访问。

---

## Phase F：删除兼容层与遗留 API

目标：彻底移除旧 MI 对象语义。

工作项：

1. 删除 `AcquireMaterialInstance` 及相关 `Spec/Key/Stats` 中的语义耦合字段。
2. 删除 `VKMaterialInstance.h/.cpp`（若 MI 已被新句柄类型替代）。
3. 清理所有旧 include 与 CMake 列表。
4. 更新主计划文档与迁移说明。

验收标准：

- 全仓库无 `MaterialInstance` 旧语义 API 引用。
- 全量构建通过，关键示例通过。

---

## 4. 风险与控制

## 4.1 生命周期风险

风险：domain 释放后句柄访问失效。

控制：

- 引入 `domain_generation`。
- 所有入口统一做 `domain_id + generation` 校验。

## 4.2 MIT 布局偏移风险

风险：材质变体切换导致 MIT slot 映射变化。

控制：

- `InitMITLayout` 输入来自当前绑定材质的 `texture_array_slot_flags`。
- 对关键 slot（如 `BaseColor`）增加断言与日志。

## 4.3 渲染行为回归

风险：slot 与 primitive 同步不一致造成 pipeline/material 错配。

控制：

- 建立“slot hash + primitive hash”调试日志。
- 在 `BindMaterialSlot` 中进行关键字段一致性检查。

---

## 5. 验证与里程碑

## M1（完成 Phase A-B）

- Quad/Text 热路径不再通过 MI 获取 material/vil/preset。
- SceneGraph + ECS 编译通过。

## M2（完成 Phase C-D）

- Primitive 全面 slot 化。
- MI 缩减为 domain+mi_id+mit 数据结构。

## M3（完成 Phase E-F）

- domain_id 句柄化落地，支持域整体替换。
- 删除兼容层，完成最终清理。

---

## 6. 建议实施顺序（本周可执行）

1. 先改 `TextRenderPipeline` 与 `Quad*`（收益大、耦合集中）。
2. 再统一 `Primitive` 创建路径到 slot。
3. 再动 `MaterialManager/MaterialAssetRegistry` 接口瘦身。
4. 最后做 `domain_id + generation` 句柄化与兼容层删除。

此顺序可以把“编译可用状态”始终保持在可回滚区间，避免一次性大爆炸。