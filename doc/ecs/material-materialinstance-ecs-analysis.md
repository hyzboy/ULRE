# Material 与 MaterialInstance 在 ECS 中的数据整理与工作机制分析

## 1. 目标与范围

本文基于当前工程代码，说明两个核心问题：

1. `Material` 与 `MaterialInstance` 的数据在 CPU/GPU 侧如何组织。
2. 这些数据在 ECS 渲染链中如何被整理、去重、索引化并最终参与 draw。

分析覆盖以下链路：

- SceneGraph 侧：`Material` / `MaterialInstance` / `MaterialParameters`
- ECS 侧：`MaterialBatch` / `MaterialInstanceAssignmentBuffer` / `PipelineMaterialRenderer`
- 绑定侧：`RenderDescriptorBindingSystem`

---

## 2. 基础对象与职责拆分

## 2.1 Material：shader + descriptor 合约 + MI 内存池入口

`Material` 本质是一组 shader 与 descriptor 绑定规则的管理对象，关键状态包括：

- `binding_contract`：描述语义化资源需求（例如 LocalToWorld、MaterialInstance、CameraInfo）。
- `mp_array`：不同 `DescriptorSetType` 对应的 `MaterialParameters`。
- `mi_data_bytes` / `mi_max_count`：材质实例结构体字节数与最大数量。
- `mi_data_manager`：材质实例数据区（CPU 侧连续内存块管理）。

因此，`Material` 既定义“要绑定什么”，也定义“每个 MaterialInstance 的数据结构和容量”。

## 2.2 MaterialInstance：只持有实例 ID，不拷贝布局定义

`MaterialInstance` 结构很轻，关键字段：

- `material`：所属材质。
- `mi_id`：在材质实例数据区中的槽位。
- `vil`：顶点输入布局关联。

创建流程：

1. `Material::CreateMI` 从 `mi_data_manager` 申请一个 `mi_id`。
2. `MaterialInstance` 构造后仅记录这个 `mi_id`。
3. `WriteMIData` 通过 `Material::GetMIData(mi_id)` 直接写入材质实例数据区。

销毁流程：

- `MaterialInstance` 析构时调用 `material->ReleaseMI(mi_id)` 回收槽位。

这意味着实例数据是集中存放、按 ID 索引，而非每个实例各自维护独立大块结构。

## 2.3 MaterialParameters：DescriptorSet 的执行器

`MaterialParameters` 负责把资源真正写入 descriptor：

- `BindUBO/BindSSBO`
- `BindTexture/BindTextureSampler`
- `Update()` 提交 descriptor 更新

`Material::BindUBO/BindSSBO/BindTexture...` 最终都转发到对应 set 的 `MaterialParameters`。

---

## 3. ECS 侧如何整理 Material/MI 数据

## 3.1 先按 Material+Pipeline 建批

ECS 使用 `MaterialPipelineKey {material*, pipeline*}` 作为 batch key。

在 `PrimitiveBatchPipeline::BuildMaterialBatches` 中：

- 遍历 `renderItems`
- 同 key 的 item 聚合到同一个 `MaterialBatch`

这样做的结果是：

- 一个 batch 内材质与管线一致，便于最小化 pipeline/descriptor 切换。
- MI 与 Transform 的分配缓冲可以按 batch 一次性构建。

## 3.2 MaterialBatch 里对 MI 的管理

`MaterialBatch` 里包含：

- `MaterialInstanceAssignmentBuffer *mi_buffer`
- `TransformAssignmentBuffer *transform_buffer`
- `VkBuffer transform_vab_buffer`
- `DrawBatchArray draw_batches`

`FinalizeBatch` 流程中会调用 `UpdateMaterialInstanceBuffer(batch)`：

1. 若材质 `hasMI()==false`，直接跳过。
2. 若 `mi_buffer` 不存在则创建。
3. 调用 `mi_buffer->WriteItems(batch.items)` 完成本批次 MI 数据整理。

---

## 4. MaterialInstanceAssignmentBuffer 的数据整理策略

## 4.1 两类输出数据

`MaterialInstanceAssignmentBuffer` 同时产出两类缓冲：

1. `material_instance_buffer`（UBO/SSBO）
   - 存放“去重后的 MI 结构体真实数据”。
2. `material_instance_vab`（R16UI）
   - 每个 RenderItem 一个 MI 索引，作为实例率属性输入。

这与 Transform 通道完全同构：

- VAB 传 ID
- UBO/SSBO 传真实结构体数组

## 4.2 去重与索引映射

`MaterialInstanceSet` 内部维护：

- `instances`：唯一 MI 指针数组
- `index_map`：`MaterialInstance* -> uint16`

`StatMaterialInstance(items)` 会：

1. 扫描 batch 中所有 item 的 `GetMaterialInstance()`。
2. 去重后得到唯一 MI 集合。
3. 把每个唯一 MI 的数据块拷贝进 `material_instance_buffer`。

随后 `WriteItems(items)` 写入 `material_instance_vab`：

- 对每个 item，查 `mi_set.Find(mi)`，写入对应 `uint16` 索引。

最终效果：

- 同一批次中重复使用相同 MI 的 item，不重复拷贝大块 MI 数据。
- draw 时每实例只需传一个 16-bit 索引。

## 4.3 容量与边界

实现中包含以下保护：

- 材质无 MI 数据（`mi_data_bytes<=0`）时跳过 MI VAB。
- 唯一 MI 数量超过 `material->GetMIMaxCount()` 会告警。
- VAB 容量采用 2 的幂扩展。

---

## 5. Descriptor 绑定如何进入材质

## 5.1 语义驱动绑定（推荐路径）

`RenderDescriptorBindingSystem` 遍历材质 `binding_contract.requirements`，按语义分派：

- `LocalToWorld` -> `transform_buffer->BindTransform(material)`
- `MaterialInstance` -> `mi_buffer->BindMaterialInstance(material)`
- 其他语义（CameraInfo/SkyInfo/Texture/Sampler）按各自来源绑定

该方式保证绑定逻辑统一由语义驱动，而不是散落在各个 renderer 中。

## 5.2 BindMaterialInstance 的实际行为

`MaterialInstanceAssignmentBuffer::BindMaterialInstance` 根据宏选择：

- `HGL_MI_USE_SSBO`：`Material::BindSSBO(SBS_MaterialInstance...)`
- 否则：`Material::BindUBO(SBS_MaterialInstance...)`

再由 `MaterialParameters` 写入 descriptor set，最终 `Material::Update()` 生效。

---

## 6. Draw 阶段如何消费这些数据

`PipelineMaterialRenderer::Render` 的职责是“绑定状态 + 提交绘制”：

1. `BindPipeline`
2. `BindDescriptorSets(material)`
3. 对每个 DrawBatch 做：
   - 绑定几何 VAB
   - 追加 `transform_vab`
   - 追加 `material_instance_vab`（若存在）
   - 绑定 IBO（若有）
   - 直接或间接 draw

关键点：

- descriptor 里已经有 LocalToWorld / MaterialInstance 的大数组缓冲。
- 顶点输入里有 TransformID / DataIndexID（新路径可附带 TextureLayerID）实例属性。
- shader 可用 ID 去索引对应数组，拿到矩阵与材质实例参数。

---

## 7. 端到端时序（ECS 视角）

一帧内与 Material/MI 相关的主路径可总结为：

1. Collect 阶段收集 `PrimitiveComponent`，生成 `RenderItem`。
2. Batch 阶段按 `MaterialPipelineKey` 聚合到 `MaterialBatch`。
3. `FinalizeBatch`：
   - 构建 draw batches
   - 生成/更新 `mi_buffer`（去重 + UBO/SSBO + MI 索引 VAB）
   - 生成/更新 Transform 索引 VAB
4. FrameSync 阶段：`RenderDescriptorBindingSystem` 绑定 LocalToWorld 与 MaterialInstance 资源。
5. DrawSubmit 阶段：`PipelineMaterialRenderer` 绑定 VAB/IBO 并提交 draw。

这条路径形成了“批内去重 + 索引分发 + 语义绑定 + 绘制消费”的闭环。

---

## 8. 与当前设计目标的契合点

结合你在渲染文档中的目标（ID 走实例率，真实数据走 UBO/SSBO），当前实现已经满足：

1. Transform 与 MI 两条通道都采用同一种模式。
2. MI 在 batch 内进行去重，减少重复上传。
3. 绑定逻辑由 binding contract 统一驱动，易于扩展新语义。

同时，现实现状也有两个工程性约束：

1. MI 去重粒度是“单个 MaterialBatch 内”，不是跨 batch 全局去重。
2. MI 索引当前使用 `uint16` VAB，需要注意超大实例规模时的索引上限。

---

## 9. 结论

在当前 ECS 架构下：

- `Material` 负责定义与承载材质实例数据规则（布局、容量、descriptor 合约）。
- `MaterialInstance` 通过 `mi_id` 指向材质实例数据池中的槽位。
- ECS 在 batch 阶段将 MI 做去重整理，生成“真实数据缓冲 + 索引 VAB”。
- 渲染阶段通过 contract 语义绑定这些缓冲，并由 renderer 将索引属性喂给 shader。

这套机制的核心价值是把“高频实例变化”压缩成“小索引 + 集中大块数据”，在保持材质灵活性的同时提升批量渲染效率。
