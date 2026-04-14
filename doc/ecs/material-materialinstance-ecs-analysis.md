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
**MaterialManager 访问模型（P4 完成后）**

`MaterialManager` 对外提供两种材质获取路径（均需持有访问令牌或由引擎内部桥接）：

| 路径 | 函数 | 调用方 |
|------|------|--------|
| 内部桥接路径 | `AcquireMaterialInternal(...)` | Quad / Text / Gizmo / Line / Font 等引擎固定组件 |
| 令牌授权路径 | `AcquireMaterial(..., MaterialAccessToken)` | `MaterialAssetRegistry`（被 `friend` 声明的友元模块） |
| **延迟语义路径（推荐）** | `MaterialAssetRegistry::ResolveMI(...)` | ECS Collect 阶段 / 应用层 | 

原先没有令牌的五个公有 `AcquireMaterial` 重载在 P4 阶段已全部删除，引擎外部代码必须通过 `MaterialAssetRegistry::ResolveMI()` 完成材质的按需解析。
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

## 3. 语义材质 / 延迟获取路径（SemanticMaterial & ResolveMI）

> 该路径是当前工程推荐的主路径，适用于所有由 ECS 驱动的 Primitive 材质解析。

### 3.1 RegisterSemanticMaterial — 注册语义材质 ID

应用层通过 `MaterialAssetRegistry::RegisterSemanticMaterial(rec)` 将 `MaterialAssetRecord` 中的"不变部分"（material 名称、纹理配置等）注册为 `SemanticMaterialId`（`uint64_t`）。该 ID 不含运行时策略字段（pipeline preset、domain_id 等），因此可以在多帧之间保持稳定。

### 3.2 ResolveMI — 运行时解析入口

`ResolveMI` 在 ECS Collect 阶段（`RenderCollect`）被调用，签名如下：

```cpp
// 低成本路径（无 entity id，走 legacy_final_mi_cache）
PrimitiveMaterialSlot ResolveMI(SemanticMaterialId   semantic_id,
                                const RuntimeMaterialRequest &request,
                                const GeometrySignature      &geometry, ...);

// 推荐路径（Phase D；走 entity_mi_cache，保证 per-entity 稳定槽位）
PrimitiveMaterialSlot ResolveMI(uint64_t              entity_id,
                                SemanticMaterialId    semantic_id,
                                const RuntimeMaterialRequest &request,
                                const GeometrySignature      &geometry, ...);
```

**内部层级缓存流程（读优先）：**

```
ResolveMI()
│
├─ 1. 构造 VariantKey {semantic_id, request, geometry}
│      → 读 variant_cache（hit → 直接返回 handle，跳过 QuerySemanticMaterial + Acquire()）
│           counter: variant_cache_hit_count / variant_cache_miss_count
│
├─ 2. [miss] QuerySemanticMaterial(semantic_id) → MaterialAssetRecord
│
├─ 3. Acquire(rec)
│      → DMB 缓存(material_name, domain_id, texture_hash)
│      → DomainCache(material_name + domain_id)
│      → MaterialManager::AcquireMaterial(..., token)
│      返回 MaterialDomainHandle
│
├─ 4. 写 variant_cache[key] = handle
│
├─ 5. 构造 EntityVariantKey {entity_id, semantic_id, request_hash, geometry_hash}
│      → 读 entity_mi_cache（hit → 复用已有 PrimitiveMaterialSlot）
│           counter: entity_resolve_hit_count / entity_resolve_miss_count
│
└─ 6. [miss] 创建新 PrimitiveMaterialSlot，写 entity_mi_cache，返回
```

### 3.3 RuntimeMaterialRequest 与 GeometrySignature

`RuntimeMaterialRequest` 封装运行时策略字段（`pipeline`、`domain_id`、`policy_flags`、`transparency_mode`、`lod_tier`），这些字段参与 `VariantKey` 哈希，不参与 `SemanticMaterialId`。

`GeometrySignature` 编码几何差异：

| 字段 | 含义 |
|------|------|
| `primitive` | 图元类型（Triangles/Lines 等） |
| `vil_hash` | VIL 中材质需要的顶点属性的 FNV-1a 哈希；0 表示尚未解析（deferred 路径） |
| `geometry_layout_hash` | 每条 VAB 的（format, stride）哈希；**仅当 `vil_hash == 0` 时**参与 `operator==`（即作为 deferred 路径的 proxy 区分） |

> **设计约束**：一旦 `vil_hash != 0`（VIL 已解析），`geometry_layout_hash` 退出等值比较——同一材质 VIL 但拥有额外 unused 属性的 Primitive 可以共享同一个 variant_cache 条目。

### 3.4 EntityVariantKey（4 字段复合键）

```cpp
struct EntityVariantKey
{
    uint64_t           entity_id;
    SemanticMaterialId semantic_id;
    uint64_t           request_hash;   // FNV-1a of RuntimeMaterialRequest
    uint64_t           geometry_hash;  // geometry_layout_hash | (vil_hash << 32)
};
```

每个 entity 可以对同一个 `semantic_id` 同时持有不同的 variant 槽位（例如不同 LOD 层级），通过 `request_hash` / `geometry_hash` 区分。

---

## 4. ECS 侧如何整理 Material/MI 数据

## 4.1 先按 Material+GraphicsPipeline 建批

ECS 使用 `MaterialPipelineKey {material*, pipeline*}` 作为 batch key。

在 `PrimitiveBatchPipeline::BuildMaterialBatches` 中：

- 遍历 `renderItems`
- 同 key 的 item 聚合到同一个 `MaterialBatch`

这样做的结果是：

- 一个 batch 内材质与管线一致，便于最小化 pipeline/descriptor 切换。
- MI 与 Transform 的分配缓冲可以按 batch 一次性构建。

## 4.2 MaterialBatch 里对 MI 的管理

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

## 5. MaterialInstanceAssignmentBuffer 的数据整理策略

## 5.1 两类输出数据

`MaterialInstanceAssignmentBuffer` 同时产出两类缓冲：

1. `material_instance_buffer`（UBO/SSBO）
   - 存放“去重后的 MI 结构体真实数据”。
2. `material_instance_vab`（R16UI）
   - 每个 RenderItem 一个 MI 索引，作为实例率属性输入。

这与 Transform 通道完全同构：

- VAB 传 ID
- UBO/SSBO 传真实结构体数组

## 5.2 去重与索引映射

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

## 5.3 容量与边界

实现中包含以下保护：

- 材质无 MI 数据（`mi_data_bytes<=0`）时跳过 MI VAB。
- 唯一 MI 数量超过 `material->GetMIMaxCount()` 会告警。
- VAB 容量采用 2 的幂扩展。

---

## 6. MaterialCache — ECS Collect 阶段的两级缓存

> 文件：`inc/hgl/ecs/support/MaterialCache.h`

`MaterialCache` 是 `RenderPrimitiveCollectSystem` 在 `RenderCollect` 阶段使用的帧级缓存，目的是让 `ResolveMI` 和 `BindMaterialSlot` 只在真正需要时才执行。

### 6.1 L1（全局 variant 哈希集合）

```cpp
std::unordered_set<uint64_t> resolved_variants;
```

只要某个 `variant_hash` 已出现在集合中，就意味着对应的 `MaterialDomainHandle` / `Material` / `VIL` 本帧内有效，任何 Primitive 遇到相同 hash 都可以跳过 `ResolveMI`（仅需复用已有材质状态）。该集合跨所有 Primitive 共享。

计数器：`frame_l1_hit` / `frame_l1_miss`

### 6.2 L2（per-Primitive 绑定状态）

```cpp
std::unordered_map<const Primitive*, PrimitiveBindingState> primitive_binding;

struct PrimitiveBindingState {
    uint64_t bound_variant_hash   = 0;
    uint32_t geometry_layout_hash = 0;
    bool     valid                = false;
};
```

记录每个 `Primitive` 上次绑定的 `variant_hash` 与 `geometry_layout_hash`。若两者未变化，`BindMaterialSlot` 调用可以跳过（`frame_l2_bind_skip`）。若几何布局发生变化（换了 VDM 或 VIL），`valid` 被设为 `false`，强制重新绑定（`frame_geometry_invalidate`）。

### 6.3 生命周期

| 方法 | 时机 |
|------|------|
| `BeginFrame()` | 每帧开始，清空帧计数器（不清 L1/L2 内容） |
| `MarkVariantResolved(hash)` | ResolveMI 成功后插入 L1 |
| `ProbeVariant(hash)` | Collect 阶段每个 Primitive 进入时检查 L1 |
| `ProbePrimitiveBinding(p, vh, glh)` | 检查 L2，顺带验证 geometry_layout_hash |
| `MarkPrimitiveBound(p, vh, glh)` | BindMaterialSlot 成功后更新 L2 |
| `ErasePrimitiveBinding(p)` | Primitive 销毁或材质强制切换时失效 L2 条目 |

### 6.4 帧统计计数器

| 计数器 | 含义 |
|--------|------|
| `frame_l1_hit` | 本帧 L1 命中次数（全局 variant 已知，跳过 Acquire） |
| `frame_l1_miss` | 本帧 L1 未命中次数（需要走 ResolveMI 完整路径） |
| `frame_l2_bind_skip` | 本帧 L2 命中次数（Primitive 绑定未变化，跳过 BindMaterialSlot） |
| `frame_geometry_invalidate` | 本帧因几何布局变化导致 L2 失效次数 |

---

## 7. Descriptor 绑定如何进入材质

## 7.1 语义驱动绑定（推荐路径）

`RenderDescriptorBindingSystem` 遍历材质 `binding_contract.requirements`，按语义分派：

- `LocalToWorld` -> `transform_buffer->BindTransform(material)`
- `MaterialInstance` -> `mi_buffer->BindMaterialInstance(material)`
- 其他语义（CameraInfo/SkyInfo/Texture/Sampler）按各自来源绑定

该方式保证绑定逻辑统一由语义驱动，而不是散落在各个 renderer 中。

## 7.2 BindMaterialInstance 的实际行为

`MaterialInstanceAssignmentBuffer::BindMaterialInstance` 根据宏选择：

- `HGL_MI_USE_SSBO`：`Material::BindSSBO(SBS_MaterialInstance...)`
- 否则：`Material::BindUBO(SBS_MaterialInstance...)`

再由 `MaterialParameters` 写入 descriptor set，最终 `Material::Update()` 生效。

---

## 8. Draw 阶段如何消费这些数据

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
- 顶点输入里有 TransformID / MaterialInstanceID 两个实例属性。
- shader 可用 ID 去索引对应数组，拿到矩阵与材质实例参数。

---

## 9. 端到端时序（ECS 视角）

一帧内与 Material/MI 相关的主路径可总结为：

1. **Collect 阶段**（`RenderCollect`）：
   - `MaterialCache::BeginFrame()` 清空帧计数器。
   - 遍历每个可见 `PrimitiveComponent`：
     - `MaterialCache::ProbeVariant(variant_hash)` → L1 命中则跳过 ResolveMI。
     - L1 未命中 → `MaterialAssetRegistry::ResolveMI(entity_id, semantic_id, request, geometry)` → 完整解析，写 variant_cache → `MaterialCache::MarkVariantResolved`。
     - `MaterialCache::ProbePrimitiveBinding(p, hash, glh)` → L2 命中则跳过 BindMaterialSlot。
     - L2 未命中 → `Primitive::BindMaterialSlot(slot)` → `MaterialCache::MarkPrimitiveBound`。
   - 生成 `RenderItem`。
2. **Batch 阶段**（`RenderBatch`）：按 `MaterialPipelineKey` 聚合到 `MaterialBatch`。
3. **FinalizeBatch**：
   - 构建 draw batches
   - 生成/更新 `mi_buffer`（去重 + UBO/SSBO + MI 索引 VAB）
   - 生成/更新 Transform 索引 VAB
4. **FrameSync 阶段**：`RenderDescriptorBindingSystem` 绑定 LocalToWorld 与 MaterialInstance 资源。
5. **DrawSubmit 阶段**：`PipelineMaterialRenderer` 绑定 VAB/IBO 并提交 draw。

这条路径形成了"语义注册 → 两级缓存加速 + 批内去重 + 索引分发 + 语义绑定 + 绘制消费"的闭环。

---

## 10. 与当前设计目标的契合点

结合渲染文档中的目标（ID 走实例率，真实数据走 UBO/SSBO），当前实现已经满足：

1. Transform 与 MI 两条通道都采用同一种模式。
2. MI 在 batch 内进行去重，减少重复上传。
3. 绑定逻辑由 binding contract 统一驱动，易于扩展新语义。
4. 材质获取路径通过 `SemanticMaterialId` + `ResolveMI` 完成延迟解析，消除了 Collect 阶段对 `MaterialManager` 公有接口的直接依赖（P4 完成后前者已删除）。
5. `MaterialCache` 两级缓存将稳定帧（材质不变、几何不变）的 `ResolveMI` 和 `BindMaterialSlot` 开销降到接近零。
6. `EntityVariantKey` 4 字段复合键保证每个 entity 的不同 variant（LOD/pipeline）可以独立持有稳定 MI 槽位。

当前工程性约束：

1. MI 去重粒度是"单个 MaterialBatch 内"，不是跨 batch 全局去重。
2. MI 索引当前使用 `uint16` VAB，需要注意超大实例规模时的索引上限。
3. `MaterialCache` 的 L1 `resolved_variants` 集合不会在帧内主动清空，依赖 `BeginFrame()` 重置帧计数器；如需在帧内强制失效（材质热重载）需额外调用 `ErasePrimitiveBinding`。

---

## 11. 结论

在当前 ECS 架构下：

- `Material` 负责定义与承载材质实例数据规则（布局、容量、descriptor 合约）。
- `MaterialInstance` 通过 `mi_id` 指向材质实例数据池中的槽位。- `MaterialAssetRegistry` 是材质获取的唯一权威入口：`RegisterSemanticMaterial` 注册语义 ID，`ResolveMI` 在运行时分层缓存（`variant_cache` → `entity_mi_cache`）解析最终槽位。
- `MaterialCache` L1/L2 是 Collect 阶段的帧级加速层，屏蔽了稳定帧的重复解析与绑定开销。- ECS 在 batch 阶段将 MI 做去重整理，生成“真实数据缓冲 + 索引 VAB”。
- 渲染阶段通过 contract 语义绑定这些缓冲，并由 renderer 将索引属性喂给 shader。

这套机制的核心价值是把“高频实例变化”压缩成“小索引 + 集中大块数据”，在保持材质灵活性的同时提升批量渲染效率。
