# ECS 材质实例数据链路（BDA 收口版）

本文档描述当前 ECS 渲染主线。材质 payload 和纹理引用不再通过材质 descriptor
索引表或 `set 2` 传递，而是通过材质专用共享 SSBO 分配行并以 BDA 地址写入
每个 draw item 的地址行。

---

## 1. 架构边界

- `MaterialRecipe` 只保留一个 `MaterialSSBOBinding`：
  `MaterialSSBOType` 选择材质结构，`ssbo_id` 标识共享物理 buffer，
  `data_index` 标识该实例在 arena 中的行。
- `MaterialSSBOBufferRegistry` 为每个 `MaterialSSBOType` 管理一个共享材质
  `DeviceBuffer`；
  `MaterialSSBODataAccessor<T>` 负责申请、写入、提交和释放一行。
- `PrimitiveBatchPipeline` 负责批次整理、排序和地址行表构建。
- `RenderPrimitiveCollectSystem` 负责把材质行地址和纹理引用行地址写入批次。
- `PipelineMaterialRenderer` 只负责管线、几何资源和 draw 提交。

材质 payload 在 `ShaderResourceSchema` 中仍保留
`DescriptorSemantic::MaterialPrivateData` 条目；其中通用 `ssbo_type` 固定为
`SSBOType::UserDefined` 只是 schema 兼容字段，具体材质结构只由
`MaterialSSBOType` 表达。材质行表需求由
`ShaderResourceSchema::requires_runtime_data_rows` 表达，不再伪装成 descriptor
资源。

---

## 2. Recipe 与共享材质行

材质实例创建 accessor 后，将其 `MaterialSSBOBinding` 写入 recipe。多个实例可以
占用同一类型共享 buffer 的不同 `data_index`；这不是 recipe 的多 slot 或多
binding 路径，而是共享材质行的生命周期管理。

recipe hash 只包含材质类型和物理 `ssbo_id` 等稳定绑定信息，不包含会随实例分配
变化的行号。实例行地址在 ECS 收集阶段按当前 binding 解析。

---

## 3. Collect 与 Batch

`RenderPrimitiveCollectSystem` 按有效 recipe 解析唯一材质 binding：

1. 通过 `MaterialSSBOBufferRegistry` 找到共享 row buffer；
2. 计算 `gpu_base + data_index * row_bytes`；
3. 把 payload BDA 写入当前 draw item 的 `MaterialInstanceAddresses` 行；
4. 若材质声明纹理引用，再写入对应的 texture-reference BDA。

`PrimitiveBatchPipeline` 为每个 draw item 保留一行地址数据。shader 通过
`pc_root.addr_mtl_data_addrs` 和 `MTL_ROW(i)` / `MTL_TEX(i)` 访问它们；不再生成
旧的 `MTL_DATA`、材质 index resolver、静态材质 payload descriptor 或 set 2
材质绑定。

---

## 4. ShaderGen 约束

provider metadata 中的材质 token 解析为：

```text
MaterialDefinition.material_private_data -> MaterialSSBOType
SerializedDescriptorEntry.ssbo_type      -> SSBOType::UserDefined
```

ShaderGen 根据 `MaterialSSBOType` 发射对应的 BDA row struct。是否需要运行时地址
行由材质 payload/纹理声明直接判定；descriptor contract 只保留真实的材质
payload语义和其它全局资源，不再包含材质数据索引语义。

---

## 5. 端到端时序

1. Recipe：声明一个 `MaterialSSBOBinding`。
2. Collect：收集可见 `RenderItem` 并解析材质 arena 行地址。
3. Batch：按 `MaterialPipelineKey` 聚合，生成每个 draw item 的地址行。
4. Shader：通过 BDA 解引用 payload 和纹理引用。
5. DrawSubmit：`PipelineMaterialRenderer` 提交几何 draw。

---

## 6. 收口验收

以下条件应保持成立：

1. 源码与头文件不再出现 `MaterialPrivateDataIndex`、旧材质 index resolver 或
   `RecipeSSBOAssetBinding`；
2. 一个 recipe 只有一个材质 payload binding；
3. `MaterialSSBOBufferRegistry` 仍支持同一材质类型的多实例、多 row；
4. ShaderGen 输出包含 `MaterialInstanceAddresses` / `MTL_ROW` 或 `MTL_TEX`
   所需的 BDA 声明，并不包含旧 `MTL_DATA` / set 2 材质资源。
