# ECS 材质实例索引链路（2026-07 收口版）

本文档描述当前 ECS 渲染主线：**不再使用 `mi_buffer`/`MaterialInstanceAssignmentBuffer`**，统一走独立 SSBO 资源与 `SSBOType + ssbo_id` 精确绑定。

---

## 1. 当前架构边界

- `PrimitiveBatchPipeline` 负责批次整理、排序和绘制批次构建。
- `RenderDescriptorBindingSystem` 负责按 binding contract 语义执行 descriptor 绑定。
- `PipelineMaterialRenderer` 只负责 VAB/IBO 与 draw 提交，不再承担材质实例数据整理逻辑。

核心约束：

1. 绑定资源解析只认 `SSBOType + ssbo_id (+ slot)`。
2. `MaterialTextureLayerTable` / `MaterialDataIndexTable` 不再 `id=0` 回退。
3. recipe 负责“声明关联”，资源存在性由独立 SSBO 注册链路保证。

---

## 2. 批处理阶段（Batch）

`PrimitiveBatchPipeline::BuildMaterialBatches` 仍按 `MaterialPipelineKey{material,pipeline}` 聚合 `RenderItem`，并在 `FinalizeBatch` 中完成：

1. batch 内 item 排序与 draw range 归并；
2. `LocalToWorldIndexTable` 相关每批行表写入；
3. 绘制命令缓冲（ICB）准备。

已移除：

- `UpdateMaterialInstanceBuffer(batch)`；
- `MaterialBatch::mi_buffer` 字段及其生命周期管理。

---

## 3. Descriptor 绑定阶段（FrameSync / DrawOnly）

`RenderDescriptorBindingSystem::ApplyContractBindings` 按语义分派：

- `LocalToWorld` / `LocalToWorldIndexTable`：从 Transform 相关缓冲或 domain 资源解析。
- `MaterialTextureLayerTable`：按 `SSBOAddress{req.ssbo_type, req.ssbo_id, texture_slot}` 解析。
- `MaterialDataIndexTable`：按 `SSBOAddress{req.ssbo_type, req.ssbo_id, data_slot}` 解析。
- 纹理/采样器语义按注册表绑定。

已移除：

1. `batch->mi_buffer->BindMaterialInstance(material)` 分支；
2. 从 `mi_buffer` 直接取 `GetTextureLayerRowsBuffer/GetDataIndexRowsBuffer` 的旁路；
3. `CompatibilityId0Fallback` 开关、统计与日志回退路径。

---

## 4. Draw 阶段

`PipelineMaterialRenderer::Render` 当前职责：

1. `BindPipeline`；
2. `BindDescriptorSets(material)`；
3. 遍历 `DrawBatch` 绑定几何 VAB/IBO 并提交 draw（直接或间接）。

注意：渲染器接口已删除 `MaterialInstanceAssignmentBuffer*` 参数，材质实例相关绑定完全由 descriptor 系统在前序阶段完成。

---

## 5. 端到端时序

1. Collect：收集可见 `RenderItem`。
2. Batch：按 `MaterialPipelineKey` 聚合并生成 draw batches。
3. FrameSync/RenderDrawOnly：`RenderDescriptorBindingSystem` 严格按 contract + `SSBOType + ssbo_id` 绑定资源。
4. DrawSubmit：`PipelineMaterialRenderer` 提交绘制。

---

## 6. 验收要点

以下条件同时满足即为主线收口完成：

1. 主仓 `inc/src` 不再出现 `mi_buffer` 与 `MaterialInstanceAssignmentBuffer` 引用；
2. 不再存在 `CompatibilityId0Fallback` 相关 API/状态；
3. `Material*Table` 绑定仅按请求 `ssbo_id` 命中，无隐式回退；
4. 示例 `03_auto_merge_material_instance` 可构建并运行。
