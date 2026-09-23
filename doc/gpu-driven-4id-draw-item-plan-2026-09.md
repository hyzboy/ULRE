# 材质数据全局池化 + 4-ID Draw Item 大计划（技术文档）

> 基线：**2026-09-23**（HEAD `d07b85526`）——2026-09-11 纹理引用重构 + 2026-09-18 Meshlet 双轨调度 + 2026-09-19～23 全局池化/4-ID/Global 改名收口。
> 状态：数据面（阶段 1、2、3、4、6）**已全部落地并改名收口**；阶段 5（UBO 分层）**部分落地**——全局 UBO 已在且类型池基址已挪入，`addr_mesh_draw_params` 在 UBO 与 push constant 两处并存，收敛未完成。
> 目的：子会话拿到本文即可拆任务，无需重新 grep——结构、数据流、改动点、行号均以本基线核实。行号随改动漂移，**以符号定位为准**。

---

## 1. 目标与终态

把渲染数据从「每帧 CPU 重建 per-batch 行表 + 内嵌 BDA 地址」改造为「创建期全局池化 + 渲染期只给 4 个 ID」，并建立「BDA 传递方式 = 数据生命周期」的分层。

**一句话**：数据全局池化、基址分层传递（UBO / push constants）、每 primitive 一个 4-ID draw item，CPU 只写 ID。

---

## 2. 现状（唯一基线）

### 2.1 MeshDrawParams 行（112B）— `inc/hgl/graph/ShaderBufferSources.h:16-88`
X-macro 单源 `HGL_MESH_DRAW_PARAMS_FIELD_LIST`（:16-33）：
```
头部 6×4B（offset 0..20）：index_base, vertex_base, is_indexed,
                          total_vertices, char_height, first_instance
地址尾 11×uint64（offset 24 起 8B 步进）：addr_position, addr_uv, addr_ntb,
                          addr_color, addr_luminance, addr_transform_id,
                          addr_size, addr_index,
                          addr_meshlets, addr_meshlet_vertices, addr_meshlet_triangles
```
- 行距 **112B**（`MeshDrawParamsLayoutValid()` :64-86，`static_assert` :87-88）。
- 代码内注释 `:10`/`:63` 仍写「共 88B」，与 `sizeof == 112` 断言不一致（**注释残留**，建议随下次改动修）。
- `addr_transform_id` 是「per-vertex transform id 流」地址（顶点级），与 4-ID 的实例级 `transform_id` **不同**（§4）。
- 该行**已全局池化**：每 `Geometry` 创建期写一次，`GeometryID` 引用（§2.9）。

### 2.2 mesh 命令行（8B）— `inc/hgl/graph/ShaderBufferSources.h:93-127`
X-macro `HGL_MESH_DRAW_COMMAND_FIELD_LIST`：`MeshDrawCommand{ geometry_id, first_instance }`（2×uint32，`static_assert` `sizeof == 8` :127）。每 DrawBatch 一行，命令序 = 行序；GLSL 侧经 `pc_root.addr_mesh_draw_params` 的 buffer_reference（`MeshDrawCommandsRef(...).cmds[gl_DrawID]`，`src/ShaderGen/meshgen/MeshTemplateEmitter.h:269-271`）。原「112B 参数行 + ICB 命令」的每帧写行路径已退役。

### 2.3 材质字段行（纯业务 payload，无纹理句柄）— `inc/hgl/graph/ssbo/MaterialDataRows.h`
| 行结构 | 大小 | 字段 |
|---|---|---|
| `PBRSurfaceRow` | 32B | base_color(16) + metallic/roughness/normal_scale/fresnel(各4) |
| `EmissiveSurfaceRow` | 16B | color(16) |
| `TransmissionSurfaceRow` | 16B | trans_color(4) + reserved0[3](12) |

- 行定义 :20-38；`static_assert(sizeof(Row)%16==0)` + 精确大小断言 :40-45。
- 纹理引用已不在行内——由 `MaterialTextureReferencePool` 独立承载（§2.5）。
- 整行 stride 真源 `GetGlobalSSBOTypeStructStride`（`inc/hgl/graph/ssbo/GlobalSSBOTypes.h:45-58`）：
  PBR=32 / Emissive=16 / Transmission=16 / CameraInfo=656。
- 类型 ↔ 行结构映射 `GlobalRowTypeTraits<T>`（`inc/hgl/graph/ssbo/MaterialSSBOLayout.h:36-39`）；
  结构体名/行名/GLSL 名/GLSL 成员表分别为 `GetGlobalSSBOStructName`(:21) / `GetGlobalSSBORowName`(:41) /
  `GetGlobalSSBOBufferName`(:54) / `GetGlobalSSBOStructGLSL`(:65)，聚合入口 `TryGetGlobalSSBOLayout`(:76)。

### 2.4 材质数据寻址 — `MaterialInstanceAddresses`（8B 双 index）
- `mtl_data_addrs` 每 draw item 一行 = `MaterialInstanceAddresses`（`inc/hgl/graph/ShaderBufferSources.h:132-140`）：
  - `payload_index` → 材质**字段行**（PBR/Emissive/Transmission 行号）；FS `MTL_ROW(i)` 解引用。
  - `texture_reference_index` → 该实例的**纹理引用行号**；FS `MTL_TEX(i)` 解引用。
  - `static_assert(sizeof == 8)` / `offsetof` 断言 :138-140。
- FS 宏发射（`src/ShaderGen/compile/MaterialShaderEmitter.cpp`）：
  - `MTL_ROW(i)`（:291-296）= `行结构(global_addresses.addr_<type>_surface + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[i].payload_index) * uint64_t(stride))`；`addr_<type>_surface` 字段名表见 :278。
  - `MTL_TEX(i)`（:322-325）= `MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(...values[i].texture_reference_index) * uint64_t(row_stride))`。
  - `MaterialInstanceAddressesRef` 的 buffer_reference 声明 :544；`BuildMaterialSSBODeclarations` :197。
  - `fragDataIndexID` = draw item 序号（表下标），由 mesh 阶段写 varying（`src/ShaderGen/meshgen/MeshShaderVaryingGen.h:115/149/150/199`）。
- 表写入：`src/ecs/support/PrimitiveBatchPipeline.cpp`（`MaterialInstanceAddresses` 行写入区 :889-1000，4-ID 直通时 :975-976 直接取 `RenderItemDescriptor` 的 `material_id`/`texture_id`）；文本路径单行表 `src/ecs/support/text/TextRenderPipeline.cpp:528-560`。
- 字段行地址 = 池基址 + `payload_index × row_bytes`：由 `RenderPrimitiveCollectSystem.cpp:750-801` 以
  `GlobalSSBOBufferRegistry::TryGetRowBuffer(ssbo_id)`（`inc/hgl/graph/module/GlobalSSBOBufferRegistry.h:220`）
  取 `GlobalRowBufferInfo{cpu_base, gpu_base, row_bytes, row_capacity, buffer}`（:63-71）后物化。

### 2.5 纹理引用池（per-definition，已落地）
- `MaterialTextureReference`（`inc/hgl/mtl/MaterialRecipe.h:182-188`）= `{uint32 descriptor_index, array_layer}`（8B，非数组 layer=0）。
- `MaterialTextureReferenceLayout`（`:207-221`）= `{layout_hash, reference_count, row_stride(16B 对齐), max_configuration_count}`，由 `.material.toml` `resources.textures` 声明顺序 + sampler/policy 推导（`BuildMaterialTextureReferenceLayout` :415-453）。
- `MaterialTextureConfigurationRetireEpochDelay = 3`（`inc/hgl/graph/module/MaterialTextureReferencePool.h:15`）。
- `MaterialTextureReferencePool`（`inc/hgl/graph/module/MaterialTextureReferencePool.h:49-128`）：per (definition, layout) arena 行池，行 = `uvec2(descriptor_index, array_layer)` 数组，行距运行期才知道（**没有 C++ 行结构体**）；容量 = `resources.texture_configurations.max_count`（默认 1024，`DefaultMaterialTextureConfigurationCapacity`，`MaterialRecipe.h:68`）；行 0 保留零行。
  - 内部换装通用件：`ActiveRowPool row_pool`(:59)（Buffer + 行号空间，`VulkanDevice::CreateArenaBuffer`）+ `ActiveRowView row_view`(:60)；`row_generations`(:61) 记每行分配代（0=无主）。
  - 自研 `free_rows` 行栈**已删除**（全仓 0 命中）——释放/回收统一走池的 `ActiveRowPool::ReleaseDeferred` / `CollectRecyclable`（`inc/hgl/vk/buffer/ActiveRowPool.h:102`）。
  - 对外：`MakePoolKey`(static, :75) / `Acquire`(:102) / `Write`(:103) / `Retire`(:107) / `CollectRetired`(:110)。
- 池的建/查托管于 `SSBOBufferRegistry`（`inc/hgl/graph/module/SSBOBufferRegistry.h:31` `material_texture_reference_pools`；`:48-52` 两个 `FindMaterialTextureReferencePool` 重载）。
- authoring 名称化：`SetMaterialTextureResource(name, texture, sampler, kind, resource_id, array_layer)`；`SetMaterialTextureArrayLayer(name, layer)` 只作为已绑定资源的更新 helper，名称按 TOML layout 校验（`inc/hgl/ecs/components/PrimitiveComponent.h:218/225`）。
- GLSL：`MTL_TEX(dataIndex).tex_<texture_name>`（`.x`=descriptor index，`.y`=array layer）。
- 2D 与 Texture2DArray 统一注册进 `texture2DArray[]`（2D 用 layer 0）。
- **sampler 运行时创建已接入（全局预设路径）**：`ShaderLibrary/sampler.toml`（array-of-tables，出现顺序即索引）→ `mtl::SamplerPresetLibrary::Load`（`src/ShaderGen/glsl_module/SamplerPreset.cpp:85`，`GetCreateInfo` :77）→ 运行时 `BindlessTextureManager::RegisterSamplers`（`src/Vulkan/VKBindlessTextureManager.cpp:330-369`）按序 `vkCreateSampler` 写入 bindless 布局 binding=1；入口 `src/SceneGraph/render/GraphicsContext.cpp:96-118`，离线侧同源加载 `src/Tools/ShaderGen/ShaderCooker.cpp:198-213`。ShaderGen 按名发射 `#define <name>Sampler <idx>u` 宏（`MaterialShaderEmitter.cpp:119`）。纹理统一为 `texture2DArray[]`(binding=0) / `sampler[]`(binding=1) / `textureCubeArray[]`(binding=2)，见 `ShaderLibrary/common/bindless_textures.glsl:40-45`；运行时重建单 sampler 走 `RebuildSampler`（`VKBindlessTextureManager.cpp:373`）。
  - **未接入的部分**：per-材质 `MaterialTextureSamplingOptions`（`MaterialRecipe.h:107`，`MaterialTextureDeclaration::sampling` :203）仅参与布局 hash，尚未驱动运行时 sampler 创建。

### 2.6 两个地址载体（BDA 分层现状）
**① `GlobalAddresses` UBO（Set 0 / binding 4，56B = 7×uint64）** — 启动写一次，仅 RenderItem/DrawItemID 两项每帧刷。
- C++ 真源 `inc/hgl/graph/ubo/GlobalAddresses.h:14-25`：`addr_mesh_draw_params, addr_pbr_surface, addr_emissive_surface, addr_transmission_surface, addr_global_render_items, addr_draw_item_ids, addr_camera_info`（`static_assert(sizeof == 56)` :25）。
- GLSL block 名 `GlobalAddressesInfo`：`ShaderLibrary/ubo/scene_ubo.glsl:88-97`（`layout(set=SCENE_SET, binding=GLOBAL_ADDRESSES_BINDING)`），宏名来源 `inc/hgl/graph/ubo/UBOShaderSources.h:39` + `inc/hgl/common/DescriptorSetTypeDef.h:110`。相机走了 UBO 索引：`#define camera CameraInfoBufferRef(global_addresses.addr_camera_info).cameras[pc_root.camera_id]`（`scene_ubo.glsl:123`）。
- 写者：`GlobalSSBOBufferRegistry::InitializeGlobalAddressesUBO`（`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp:85-121`，写 5 个池基址）+ `UpdateRenderItemAddresses`（`:128`，每帧刷 RenderItem/DrawItemID）。
- 每帧刷新点：`RenderSceneUBOSystem::ResolveGlobalAddressesUBO`（`src/ecs/systems/render/RenderSceneUBOSystem.cpp:152-180`）+ `UpdateUBO`(:387-400)。

**② `RootAddresses` push constant（72B）** — 每 MaterialBatch draw 前下发。
- 真源 `inc/hgl/graph/ShaderBufferSources.h:206-264`，X-macro `HGL_ROOT_ADDRESSES_FIELD_LIST`：
```
addr_mesh_draw_params, addr_l2w, addr_l2w_index, addr_mtl_data_addrs,
addr_texture_references, addr_text_char_info, addr_text_char_style,
addr_text_char_instance, camera_id, _pad_camera
```
  布局断言 8×uint64 连续 + 2×uint32（`sizeof == 72`）：:246-264。
- 下发 `graph::PushRootAddresses`（`inc/hgl/graph/RootAddressPush.h:28`）；三路径调用点：`src/ecs/support/PipelineMaterialRenderer.cpp:173`、`src/ecs/support/line/LineRenderPipeline.cpp:762`、`src/ecs/support/text/TextRenderPipeline.cpp:290`。

### 2.7 全局池 SSBO 管理 — `GlobalSSBOBufferRegistry`
- 类型枚举 `GlobalSSBOType`（`inc/hgl/graph/ssbo/SSBOTypes.h:14-25`）：`MeshDrawParams / PBRSurface / EmissiveSurface / TransmissionSurface / CameraInfo`（原 `mtl::MaterialSSBOType` 双枚举已并入并删除）。
- 表驱动预分配（`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp:13-20`）：
  | 类型 | 行大小 | 默认容量 | 预留行 |
  |---|---|---|---|
  | `MeshDrawParams` | `sizeof(mtl::MeshDrawParams)`=112B | **16384** | 1 |
  | `PBRSurface` | `sizeof(ssbo::PBRSurfaceRow)`=32B | 1024 | 1 |
  | `EmissiveSurface` | 16B | 1024 | 1 |
  | `TransmissionSurface` | 16B | 1024 | 1 |
  | `CameraInfo` | `sizeof(CameraInfo)` | 64 | 1 |
- 每类型一个 `ActiveRowPool pools[GlobalSSBOTypeCount]`（`inc/hgl/graph/module/GlobalSSBOBufferRegistry.h:88`）；Arena 一次性分配、终身不重建、BDA 恒定（`CreatePool` :100/`InitializePools` :99）。
- 访问器 `GlobalSSBODataAccessor : public ActiveRowLease`（:31-61）= 行租约 + `{global_ssbo_type, ssbo_id}`；`GetGlobalSSBOBinding()` 返回 `{type, ssbo_id, GetRowID()}`。
- `GlobalSSBOBinding{ ssbo_type, ssbo_id, data_index }`（`inc/hgl/graph/ssbo/GlobalSSBOTypes.h:64-76`，`IsValid()` :70）；`GlobalSSBOConfig`（:78-85）。
- 取行：`GlobalSSBODataAccessor GetAccessor(GlobalSSBOType)`(:189) / 模板 `GetAccessor<T>()`(:201-218，经 `GlobalRowTypeTraits<T>::TYPE` 且校验 `GetRowBytes() == sizeof(T)`)；`TryGetRowBuffer(uint32_t ssbo_id, GlobalRowBufferInfo&)`(:220，实现 `GlobalSSBOBufferRegistry.cpp` 内)。
- MeshDrawParams 便捷接口 :224-256：`Acquire(params)` / `Write(id, params)` / `ReleaseID(id)` / `GetMeshDrawParamsGPUBase()` / `GetRowGPU(id)` / `GetMeshDrawParamsBuffer()`。
- `MeshDrawParamsPool.h`（`inc/hgl/graph/module/MeshDrawParamsPool.h`）已退化为 4 行转发头，只 `#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>`。

### 2.8 通用 SSBO 域注册 — `SSBOBufferRegistry`
- `domain_map`: `(ssbo_type, ssbo_id)` → `SSBOBufferBinding{ssbo_type, ssbo_id, buffer, element_capacity, element_stride}`（`inc/hgl/graph/module/SSBOBufferRegistry.h:16-23`，map :29）。
- 键 = `mtl::SSBOAddress{ssbo_type, ssbo_id, slot}`（`inc/hgl/graph/ssbo/SSBOTypes.h:115-125`，`MakeSSBOAddress` :122）；ID 命名空间位 `SSBOIdNamespaceBit` / `MakeRecipeSSBOId` / `MakeECSSSBOId` / `IsECSSSBOId`（:86-113）。
- 该 registry 只服务通用非材质 SSBO：`Touch`(:58) / `RegisterBuffer`(:60) / `ClearDomain`(:62) / `HasBinding`(:64) / `TryGetBinding`(:65) / `GetElementCapacity`(:67)。
- 旧分配链 `next_ssbo_id` + `AllocateSSBOId()` + `AllocateArrayAccessor<T>()` + `row_segments`/`RowSegmentInfo` **已全部删除**（全仓 0 命中）——SSBO 分配上收至 `BufferManager`，本 registry 改为「只登记已存在的绑定」。
- 同时托管纹理引用池：`material_texture_reference_pools`(:31) + `FindMaterialTextureReferencePool`(:48-52)；材质纹理配置 CRUD :76-102。
- `null_row_buffer`（64B 零填充空行，`GetNullRowAddress()` :113，声明 :33）——地址表「无有效行」安全缺省。

### 2.9 几何：MeshDrawParams 池化 + Meshlet 双轨（已落地）
- `Geometry` 持 `geometry_id`（`inc/hgl/graph/geo/VKGeometry.h:121`，`GetGeometryID`/`SetGeometryID` :126-127）；`GeometryDataBuffer`（`inc/hgl/graph/mesh/GeometryDataBuffer.h:13-112`）亦持同源镜像字段 `geometry_id`(:15)，比较/排序用(:37/76)。
- `Geometry::EnsureMeshDrawParams(GlobalSSBOBufferRegistry*, VulkanDevice*)`（`VKGeometry.h:130`）：**短路幂等**——`geometry_id != 0` 直接返回 `true`；否则转调 `RegisterMeshDrawParams`。
- `Geometry::RegisterMeshDrawParams`（实现 `src/SceneGraph/VKGeometry.cpp:167-174`）：填地址尾（含 Meshlet 三地址，:157-165）后，`geometry_id == 0` → `pool->Acquire(params)` 取 `GeometryID`，已持有 → `pool->Write(geometry_id, params)` 改写真源。析构归还行号前先校验 `IsActive`（`VKGeometry.cpp:24-36`）。
- `MeshDrawParams` 行**追加 3 个 meshlet BDA**（112B，§2.1）——meshlet 网格与常规网格**共用同一行结构与池**。
- Meshlet GPU Storage Buffer：`Geometry` 持 `meshlets_buffer` / `meshlet_vertices_buffer` / `meshlet_triangles_buffer` / `meshlet_bounds_buffer` + `meshlet_count`（`VKGeometry.h:95-99`），`SetMeshlets(...)`(:110-118)、`HasMeshlets()`(:103)；结构 `MeshletDescriptor`(16B, :17-26) / `MeshletBounds`(:28)。
- `PrimitiveBatchPipeline` 自动双轨分流：含 Meshlet 的网格按 `meshlet_count` 发射；常规网格维持 `VertexPassthrough` 调度（`PrimitiveBatchPipeline.cpp:644-700` 写 `MeshDrawCommand` 时按 `db.geometry_id`/`geom_data_buffer->geometry_id` 填 `geometry_id`）。
- 发射侧：`MeshShaderHeaderGen` 声明 8-bit storage 扩展（`src/ShaderGen/meshgen/MeshShaderHeaderGen.h:55`）；`MeshShaderVertexAdapter` 声明 Meshlet BDA `buffer_reference`。
- **对 4-ID 的约束**：命令面覆盖双轨；4-ID 的 `geometry_id` 指向的行自带 meshlet 三地址，无需额外 ID 通道。

### 2.10 4-ID 链路（已落地）
| 环节 | 符号 | 位置 |
|---|---|---|
| CPU 描述符 | `struct RenderItemDescriptor{transform_id, geometry_id, material_id, texture_id}`（16B） | `inc/hgl/graph/render/RenderItemDescriptor.h:19-44`（`static_assert(sizeof==16)` :41；`RenderItemHandle`/`INVALID_RENDER_ITEM_HANDLE` :43-44） |
| 别名与 X-macro 形态 | `DrawItem4ID` + `HGL_DRAW_ITEM_4ID_FIELD_LIST`（16B） | `inc/hgl/graph/ShaderBufferSources.h:148-186` |
| 全局池 | `RenderItemDataStorage`（CPU 镜像 + `free_list` + 脏范围 + GPU SSBO 镜像） | `inc/hgl/ecs/support/RenderItemDataStorage.h:29-129`（`Allocate`/`AllocateContiguous` :69、`Set4ID` :91、`EnsureGPUBuffer` :117、`SyncToGPU` :120） |
| 二级绘制索引 | `DrawItemIDStorage` | `inc/hgl/ecs/support/DrawItemIDStorage.h` |
| 连号折叠 | `kRenderItemIndexedFlag=0x80000000u`、`CompactedDrawRange`、`CompactRenderItemHandles` | `inc/hgl/ecs/support/DrawItemCompaction.h:15/32/64` |
| 组件落地 | `PrimitiveComponent::render_item_descriptor`、`Set4ID`、`GetRenderItemDescriptor` | `inc/hgl/ecs/components/PrimitiveComponent.h:140/251/257` |
| 实例化组件 | `InstancedPrimitiveComponent::AllocateContiguousInstances` / `SetAllInstances4ID` | `inc/hgl/ecs/components/InstancedPrimitiveComponent.h` |
| 登记点 | `RenderPrimitiveCollectSystem.cpp:1418/1426` `primitiveComp->Set4ID(transform_id, geometry_id, material_id, texture_id)` | — |
| GLSL 解析 | `ResolveRenderItemDirect`(:73) / `ResolveRenderItemIndexed`(:79) / `ResolveRenderItemAuto`(:112) / `RENDER_ITEM_INDEXED_FLAG`(:99) / `global_render_items`·`draw_item_ids` 宏(:69-70) | `ShaderLibrary/common/RenderItemResolve.glsl`（142 行） |
| 包围盒（CS 剔除用） | `GeometryAABB`（32B：center.xyz+r、extents.xyz） | `inc/hgl/graph/ShaderBufferSources.h:188-198` |
| 端到端示例 | `ComputeFrustumCull.cpp`（CS 剔除 + 无锁紧凑 + 间接命令）、`ComputeAsteroidBelt.cpp`（10 万实例） | `example/Basic/` |

---

## 3. 终态数据模型（现状即终态）

```
┌─ 全局 UBO：GlobalAddressesInfo（Set 0 / binding 4，56B，启动写一次）──┐
│  addr_mesh_draw_params        MeshDrawParams 池基址                  │
│  addr_pbr_surface             PBRSurface 池基址                      │
│  addr_emissive_surface        EmissiveSurface 池基址                 │
│  addr_transmission_surface    TransmissionSurface 池基址             │
│  addr_camera_info             CameraInfo 池基址（camera_id 索引）     │
│  addr_global_render_items     RenderItemDescriptor 池基址（每帧刷）   │
│  addr_draw_item_ids           DrawItemID 二级索引表基址（每帧刷）     │
└──────────────────────────────────────────────────────────────────────┘

┌─ push constants：RootAddresses（72B，每 MaterialBatch draw 前）──────┐
│  addr_mesh_draw_params        仍与 UBO 并存（阶段 5 收敛未完成）      │
│  addr_mtl_data_addrs          每材质 MaterialInstanceAddresses 行表基址│
│  addr_texture_references      每材质纹理引用池基址（per-definition）  │
│  addr_l2w / addr_l2w_index    per-world L2W 数据 / 索引表            │
│  addr_text_char_info/_style/_instance   文本三表                     │
│  camera_id (+_pad_camera)     全局 CameraInfo 池行号                 │
└──────────────────────────────────────────────────────────────────────┘

┌─ 4-ID draw item（16B，per draw item）───────────────────────────────┐
│  RenderItemDescriptor{ transform_id, geometry_id, material_id,       │
│                        texture_id }                                  │
│  现 ReBAR CPU 直写（RenderItemDataStorage::SyncToGPU）；             │
│  CS 写路径已在 ComputeFrustumCull 打通（未接引擎 ECS 主路径）          │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 4. ID 语义词典

| ID | 含义（终态） | 现状对应 | 备注 |
|---|---|---|---|
| `transform_id` | L2W 池行号（实例变换） | `l2w_index[gl_InstanceIndex]` 查行；`RenderItemDescriptor::transform_id` | 与 `addr_transform_id`（顶点级）**不同** |
| `geometry_id` | MeshDrawParams 池行号（几何+绘制参数，**行距 112B**） | `Geometry::GetGeometryID()`；`MeshDrawCommand.geometry_id`；`RenderItemDescriptor::geometry_id` | **已落地**；Meshlet 网格走双轨独立发射（§2.9） |
| `material_id` | 材质参数寻址中间层 index | `RenderItemDescriptor::material_id` → `MaterialInstanceAddresses.payload_index` → `global_addresses.addr_<type>_surface + index×stride` | **已落地**（原 `payload_address` 地址已退役） |
| `texture_id` | 纹理引用行号 | `RenderItemDescriptor::texture_id` → `MaterialInstanceAddresses.texture_reference_index` → `MaterialTextureReferencePool` 行 | **已落地** |

**寻址链**：
- 几何：`geometry_id` → MeshDrawParams 池行 → 顶点流地址/段偏移（`MeshDrawCommandsRef(pc_root.addr_mesh_draw_params).cmds[gl_DrawID]`）。
- 材质：`material_id` → `MaterialInstanceAddresses` 行 → `global_addresses.addr_<type>_surface + index×row_bytes` → 字段。
- 纹理：`texture_id` → `MaterialTextureReferencePool` 行 → `uvec2(descriptor_index, array_layer)` → bindless 采样。
- 变换：`transform_id` → L2W 池（静态段 + 动态段×ring）。

---

## 5. 阶段依赖图与状态

```
阶段 1（类型池化）✅ ──┐
                       ├──→ 阶段 3（双地址 → 双 index）✅ ──┐
阶段 2（MeshDrawParams 池化）✅ ─┘                            ├──→ 阶段 4（4-ID）✅ ──→ 阶段 5（UBO 分层）◐部分 ──→ 阶段 6（删链）✅
（3 依赖 1（payload index）、纹理 index 部分独立；4 依赖 1-3；5 依赖 4；6 依赖 1-5）
```
✅ = 已落地并改名收口；◐ = 部分落地（见 §6.5）。

---

## 6. 分阶段改造

### 6.1 阶段 1 — 材质字段类型池化 ~~（待执行）~~ **已完成**

**结果**：每字段类型（PBR/Emissive/Transmission）由 `GlobalSSBOBufferRegistry` 预创建一个共享 SSBO（默认 1024 行），每类由 `ActiveRowPool` 管理行 ID；`GlobalSSBODataAccessor<T>` 负责 RAII 申请/释放。`ssbo_id` 表示共享物理 buffer identity，实例隔离由 `data_index` 完成。

**实现（现状）**：
```
GetManager<GlobalSSBOBufferRegistry>()->GetAccessor<PBRSurfaceRow>()
  → 预创建共享 buffer + ActiveRowPool → GlobalSSBODataAccessor
  → GetGlobalSSBOBinding() = {ssbo_type, ssbo_id, data_index}
每实例 SetMaterialDataResource(binding)
RPC 物化 → TryGetRowBuffer(ssbo_id) → row = gpu_base + payload_index×row_bytes
```

**涉及文件**：`inc/hgl/graph/module/GlobalSSBOBufferRegistry.h/.cpp`、`inc/hgl/graph/ssbo/GlobalSSBOTypes.h`、`inc/hgl/graph/ssbo/MaterialSSBOLayout.h`、`inc/hgl/graph/ssbo/MaterialDataRows.h`、`inc/hgl/mtl/MaterialRecipe.h`、`inc/hgl/ecs/components/PrimitiveComponent.h/.cpp`、`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` 与示例 accessor 创建点。通用 `SSBOBufferRegistry` 只负责非材质 SSBO 与材质纹理引用池。

**证据**：`GlobalSSBOBufferRegistry` 143 命中 / 54 文件；`GlobalSSBODataAccessor` 48/33；`ActiveRowPool` 62/8；旧名 `MaterialSSBOBufferRegistry` / `MaterialSSBODataAccessor` / `GetMaterialDataAccessor` / `MaterialRowTypeTraits` / `MaterialSSBOType` 全仓 **0 命中**（改名收口提交 `e9f5ec8bd`、`3ea719317`）。`ActiveIDManager` 已下沉仓库、不再由材质池使用（本仓命中均为行池 `ActiveRowPool` 体系）。

---

### 6.2 阶段 2 — MeshDrawParams 池化 ~~（待执行）~~ **已完成**

**目标**：所有 primitive 几何绘制参数创建期一次性写入全局池，`geometry_id` 引用；池永久固定（预分配上限、只增不减、基址不变）。

**终态（现状）**：
- primitive 创建期 `Geometry::EnsureMeshDrawParams(pool, dev)` 写池行 → `GeometryID`（`VKGeometry.h:130`：`geometry_id != 0` 短路；实写走 `RegisterMeshDrawParams`，`VKGeometry.cpp:167-174`）。
- 渲染期命令面只引用 `geometry_id`：`MeshDrawCommand{geometry_id, first_instance}` 8B（§2.2），`WriteMeshDrawCommands` 不再写 112B 行（`PrimitiveBatchPipeline.cpp:644-700`）。
- 池容量 **16384** 行、`reserve_rows = 1`、Arena 终身不重建（`GlobalSSBOBufferRegistry.cpp:15`）。

**涉及文件**：`src/ecs/support/PrimitiveBatchPipeline.cpp`、`inc/hgl/ecs/support/PrimitiveBatchPipeline.h`、`inc/hgl/graph/ShaderBufferSources.h`、`inc/hgl/graph/geo/VKGeometry.h`、`src/SceneGraph/VKGeometry.cpp`、`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp`、`src/ecs/support/PipelineMaterialRenderer.cpp`。

**验收（已达成）**：编译 + 全部示例；MeshDrawParams 行只在创建期写一次；几何不变池零重写。

**证据**：`EnsureMeshDrawParams` 声明 `VKGeometry.h:130` / 实写 `RegisterMeshDrawParams` `VKGeometry.cpp:167-174`；`MeshDrawCommand` 布局断言 `ShaderBufferSources.h:121-127`；`Geometry::geometry_id` `VKGeometry.h:121`、`GeometryDataBuffer::geometry_id` `inc/hgl/graph/mesh/GeometryDataBuffer.h:15`；容量表 `GlobalSSBOBufferRegistry.cpp:15`；提交 `22f47cf8e`、`9a98eec2d`。

**待澄清（仍未核实）**：① `geometry_id` 粒度——现为 **per `Geometry`**（`VKGeometry.h:121` 单槽 + `EnsureMeshDrawParams` 短路），运行时多实例区分靠 `first_instance`；是否需要 per 运行时实例另立 ID 未见证据。② `addr_transform_id`（顶点级 id 流）是否一并池化——本阶段未见该字段语义变更。③ Meshlet 三地址与顶点流地址同池同规则（同在 112B 行内）——是否还需额外规则未核实。

---

### 6.3 阶段 3 — `mtl_data_addrs` 双地址 → 双 index ~~（待执行）~~ **已完成**

**目标**：`MaterialInstanceAddresses` 的两个 `uint64_t` 地址 → 两个 `uint32_t` index；「地址 → 类型池基址 + index」两级寻址。

**落地成果（现状核对）**：
1. `MaterialInstanceAddresses` 收缩为 `{ uint32_t payload_index; uint32_t texture_reference_index; }`（8B，offset/sizeof 断言齐备）——`ShaderBufferSources.h:132-140`。
2. `RootAddresses` push constants 增加 `addr_texture_references`，由 MaterialBatch 携带 per-definition 纹理引用池基址并推入（`MaterialBatch.texture_reference_base_addr`，`inc/hgl/ecs/core/MaterialBatch.h:67`；`PipelineMaterialRenderer.cpp:173` / `TextRenderPipeline.cpp:290` / `LineRenderPipeline.cpp:762` 三路径下发）。
3. FS 宏 `MTL_ROW(i)` 经 `global_addresses` UBO 的各类型基址做两级寻址（`MaterialShaderEmitter.cpp:291-296`）。
4. FS 宏 `MTL_TEX(i)` 经 push constant 纹理池基址做两级寻址（`MaterialShaderEmitter.cpp:322-325`）。
5. `PrimitiveBatchPipeline` 与 `TextRenderPipeline` 统一写 8 字节 index 行（`PrimitiveBatchPipeline.cpp:889-1000`；`TextRenderPipeline.cpp:528-560`）。
6. `forward_unlit.glsl.tmpl` 补 `#include "ubo/scene_ubo.glsl"`。
7. `ShaderResourceSchemaRegressionGate` 与 ShaderGen 回归测试覆盖 `payload_index` / `texture_reference_index` 写法（`src/Tools/ShaderGen/ShaderResourceSchemaRegressionGate.cpp:3727/3828`）。

**证据**：旧字段 `payload_address` / `texture_reference_address` 全仓 **0 命中**；新字段命中 62 处 / 17 文件。提交 `a193aeaf2`。

---

### 6.4 阶段 4 — 4-ID draw item SSBO ~~（待执行）~~ **已完成**

**目标**：每 primitive 一个 `{transform_id, geometry_id, material_id, texture_id}`（16B）紧凑结构，替代 112B 内嵌地址行。SSBO 形态，ReBAR CPU 直写（预留 CS 写）。

**终态（现状）**：
```
RenderItemDescriptor 池（SSBO，per draw item，ReBAR 直写）
ICB 命令面 → gl_DrawID → DrawItemID 二级索引（可选）→ 4-ID 行 → 4 个 ID → 各池
```
- `RenderItemDescriptor`（16B）`inc/hgl/graph/render/RenderItemDescriptor.h:19-44`；X-macro 形态 `DrawItem4ID` `ShaderBufferSources.h:148-186`。
- 池与显存镜像：`RenderItemDataStorage`（`inc/hgl/ecs/support/RenderItemDataStorage.h:29-129`），连号块分配 `AllocateContiguous`(:69)、增量脏范围 `MarkRangeDirty`(:108)/`GetDirtyRange`(:111)、`SyncToGPU`(:120)；二级索引 `DrawItemIDStorage`；连号折叠 `DrawItemCompaction.h`。
- GLSL 解析统一入口 `ShaderLibrary/common/RenderItemResolve.glsl`（直通 / 二级索引 / 自动三分支）。
- 组件与登记：`PrimitiveComponent::Set4ID`（`PrimitiveComponent.h:257`）+ `RenderPrimitiveCollectSystem.cpp:1418/1426`；实例化路径 `InstancedPrimitiveComponent`。
- CS 写路径已打通（`example/Basic/ComputeFrustumCull.cpp`：候选 4-ID → `GeometryAABB` 视锥剔除 → `atomicAdd` 紧凑写入 Visible 4-ID → 间接命令）。

**涉及文件**：`inc/hgl/graph/render/RenderItemDescriptor.h`、`inc/hgl/graph/ShaderBufferSources.h`（`DrawItem4ID`/`GeometryAABB`）、`inc/hgl/ecs/support/RenderItemDataStorage.h/.cpp`、`inc/hgl/ecs/support/DrawItemIDStorage.h`、`inc/hgl/ecs/support/DrawItemCompaction.h`、`inc/hgl/ecs/components/PrimitiveComponent.h/.cpp`、`inc/hgl/ecs/components/InstancedPrimitiveComponent.h/.cpp`、`src/ecs/support/PrimitiveBatchPipeline.cpp`、`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp`、`ShaderLibrary/common/RenderItemResolve.glsl`、`example/Basic/ComputeFrustumCull.cpp`、`example/Basic/ComputeAsteroidBelt.cpp`。

**验收（已达成）**：编译 + 全部示例；命令面只引用 ID（`MeshDrawCommand` 8B）；4-ID 直通行表写入由 `RenderItemDescriptor` 直接解析。提交 `31436ece7`、`e8e45baeb`、`9a98eec2d`、`226a592ea`、`cc97be792`、`a306d9c35`。

**待澄清（仍未核实）**：① CS 写 4-ID 是否要接入引擎 ECS 主路径（现仅示例内打通）。② `TextureID` 与 `MaterialTextureReferencePool` 行号的对应关系由 `RenderPrimitiveCollectSystem` 落地（`payload_index`/`texture_reference_index` 同源），是否需要独立持久化 ID 未见证据。

---

### 6.5 阶段 5 — UBO 分层 ◐ **部分落地（此项仍未完成）**

**目标**：类型池基址 + MeshDrawParams 基址挪入全局 UBO（一次写永久）；`mtl_data_addrs`/纹理引用池/`L2W` 基址保持 push constants。

**已落地**：
- 新 UBO **已存在**：`GlobalAddresses`（Set 0 / binding 4，56B），C++ 真源 `inc/hgl/graph/ubo/GlobalAddresses.h`，GLSL block `GlobalAddressesInfo`（`scene_ubo.glsl:88-97`），描述符表项 `DescriptorSetTypeDef.h:18/28/110`。
- **`addr_mesh_draw_params` + 类型池基址（PBR/Emissive/Transmission）+ `addr_camera_info` 已进 UBO**，启动写一次（`GlobalSSBOBufferRegistry::InitializeGlobalAddressesUBO`）；`MTL_ROW` 已改读 `global_addresses.addr_<type>_surface`。
- Scene 集布局由 `VKGlobalSceneUBOSet.cpp:29-70` 表驱动（`SceneBinding::RANGE_SIZE` 项，binding 4 = global_addresses，stageFlags 含 `VK_SHADER_STAGE_COMPUTE_BIT`）。

**未完成（本阶段剩余改动点）**：
- ~~[ ] `RootAddresses` 去掉挪走字段（`addr_mesh_draw_params`）~~ —— **未完成**：`addr_mesh_draw_params` 仍在 `HGL_ROOT_ADDRESSES_FIELD_LIST`（`ShaderBufferSources.h:207`），且三渲染路径仍传 mesh_draw_params 缓冲（`PipelineMaterialRenderer.cpp:173-178`；`TextRenderPipeline.cpp:290-301`；`LineRenderPipeline.cpp:762`）。当前是 UBO 与 push constant **两处并存**的双源状态，收敛需改 GLSL 发射（`MeshShaderHeaderGen`/`MeshTemplateEmitter` 的 `pc_root.addr_mesh_draw_params` 引用改 UBO）+ 三路径 push 调用签名。
- ~~[ ] 三渲染路径 push 调用更新~~ —— 未完成（同上）。
- ~~[ ] GLSL 发射：`pc_root` 减字段~~ —— 未完成（同上）。
- `L2W` / 纹理引用池基址是否进 UBO：**仍按决策清单留在 push constant**（§8）。

**涉及文件**：`inc/hgl/graph/ShaderBufferSources.h`、`inc/hgl/graph/ubo/GlobalAddresses.h`、`inc/hgl/graph/RootAddressPush.h`、`src/Vulkan/VKGlobalSceneUBOSet.cpp`、`src/Vulkan/pipeline/VKPipelineLayoutData.cpp`、`src/ShaderGen/meshgen/MeshTemplateEmitter.h`、`src/ShaderGen/meshgen/MeshShaderHeaderGen.h`、`src/ShaderGen/compile/MaterialShaderEmitter.cpp`、三渲染器 push 调用。

**验收（部分达成）**：编译 + 全部示例 ✓；永久地址（类型池 + mesh_draw_params）一次写 ✓；**push 面仍含 `addr_mesh_draw_params` ✗**。

**待澄清**：`addr_mesh_draw_params` 收敛后是否保留 push 侧兼容字段（现为双源，改动波及 mesh shader 全部 `MeshDrawCommandsRef(pc_root...)` 调用点）。

---

### 6.6 阶段 6 — 删 asset 绑定链 ~~（待执行）~~ **已完成**

**目标**：删除「类型即身份」后冗余的多键结构，收敛为单槽 + 类型直指。

**删除面（现状）**：
1. `PrimitiveComponent`：多键 authoring resource 收敛为单一 `MaterialDataAuthoringResource`（`inc/hgl/ecs/components/PrimitiveComponent.h:69-89`，字段 :118，`SetMaterialDataResource` :227-229）。
2. `MaterialRecipe`：多项 asset 列表收敛为唯一 `GlobalSSBOBinding material_ssbo_binding`（`inc/hgl/mtl/MaterialRecipe.h:508`）——**类型已由 `MaterialSSBOBinding` 改名 `GlobalSSBOBinding`**（旧类型名 0 命中；字段名 `material_ssbo_binding` 保留）。
3. `MaterialComponent`：删除 resolved binding 缓存与过渡期查询 API（`inc/hgl/ecs/components/MaterialComponent.h:53` 仅余 `cached_effective_recipe` + `cached_effective_recipe_hash`）。
4. `RenderPrimitiveCollectSystem`：直接消费 recipe binding（`MaterializeRecipeRowsForPrimitive` :653，`TryGetRowBuffer` :750），无多键物化过渡层。
5. 编译期 schema/契约/provider manifest：`MaterialDefinition.material_private_data` 现为 `GlobalSSBOType` 直选（`MaterialRecipe.h:299`），provider metadata 不再参与 payload 合并（`ResolveEffectiveMaterialPrivateData` 仅此相关）。
6. `MakeRecipeSSBOId(local_id)` 只保留通用 recipe SSBO identity 生成（`SSBOTypes.h:89`），配合 `MakeECSSSBOId`/`IsECSSSBOId`/`GetSSBOIdLocalPart` 命名空间。

**验收（已达成）**：编译 + gate + 全仓 grep 确认旧材质 slot 字段、多键 vector 和 slot-keyed authoring API 均无残留（`MaterialSSBOBinding`/`MaterialSSBOBufferRegistry`/`MaterialSSBODataAccessor`/`GetMaterialDataAccessor` 全 0 命中）。

---

## 7. 已完成事项 + 开放待办

### 7.1 已完成（2026-09-11 纹理引用重构）
- `tex_tail`/`TextureSlot` 固定槽/`TextureLayerRow`/`TextureRectArraySurfaceRow` 删除；材质行收敛为纯 payload（3 类型）。**证据**：`tex_tail` 0 命中、`TextureSlot` 0 命中、`TextureLayerRow` 0 命中、`TextureRectArraySurfaceRow` 0 命中。
- `MaterialTextureReferencePool`（per-definition 池）+ `MaterialTextureReferenceLayout`（TOML 声明顺序推导）。
- authoring 名称化（`SetMaterialTextureResource(name,..., array_layer)` / `SetMaterialTextureArrayLayer(name, layer)`）。
- 2D/Texture2DArray 统一进 `texture2DArray[]`；专用数组材质/provider 删除。
- `SSBOType` 枚举收敛（`TextureLayer`/`TextureRectArraySurface` 已删）——现枚举仅 `MeshDrawParams / LocalToWorld / LocalToWorldIndex / UserDefined`（`SSBOTypes.h:31-41`）。

### 7.2 已完成（2026-09-19~23 Global 改名与收口）
- ~~`MaterialInstanceAddresses`（16B 双地址）~~ —— 已收缩为 8B 双 index（§6.3）。
- ~~`MaterialSSBOBufferRegistry` / `MaterialSSBODataAccessor` / `GetMaterialDataAccessor` / `MaterialRowTypeTraits`~~ —— 全部并入 `Global*` 前缀（`e9f5ec8bd`、`3ea719317`）。
- ~~`MeshDrawParamsPool` 独立类型~~ —— 已退化为转发头（`inc/hgl/graph/module/MeshDrawParamsPool.h` 4 行）。
- ~~`SSBOBufferRegistry::AllocateSSBOId` / `AllocateArrayAccessor` / `row_segments` / `RowSegmentInfo` / `next_ssbo_id`~~ —— 已删除（全 0 命中），分配上收 `BufferManager`。
- ~~`TransformDataBuffer` 独立类型~~ —— 现为 `TransformAssignmentBuffer::GetTransformDataBuffer()`（`inc/hgl/ecs/support/TransformAssignmentBuffer.h:73`）返回的 `DeviceBuffer*`。

### 7.3 已完成（2026-09-19~21 4-ID / 实例化）
- `RenderItemDescriptor` / `RenderItemDataStorage` / `DrawItemIDStorage` / `DrawItemCompaction` 全链落地（§6.4）。
- 百万级实例示例 `example/Basic/ComputeAsteroidBelt.cpp`（`TOTAL_ASTEROIDS = GEOMETRY_VARIANT_COUNT(10) × INSTANCES_PER_GEOM(100000) = 1,000,000`，`ComputeAsteroidBelt.cpp:66-68`）、CS 剔除示例 `example/Basic/ComputeFrustumCull.cpp`。

### 7.4 开放待办（独立，不阻塞）
1. ~~**sampler 运行时创建未接入**——filter/wrap/swizzle/compare 的 TOML 配置只到「解析 + 布局 hash + 契约传递」~~——**已完成（2026-09-23）**：`ShaderLibrary/sampler.toml` → `SamplerPresetLibrary::Load`（`SamplerPreset.cpp:85`）→ `BindlessTextureManager::RegisterSamplers`（`VKBindlessTextureManager.cpp:330`）按序 `vkCreateSampler` 写 binding=1，入口 `GraphicsContext.cpp:96-118`；`max_lod`/`compare_op`/`anisotropy` 等字段齐备。**仅剩**：per-材质 `MaterialTextureSamplingOptions` 覆盖（`MaterialRecipe.h:107/203`）只参与 layout hash，尚未驱动运行时创建。
2. ~~**屏蔽示例恢复**~~——**已完成**：`LoadGeometry`/`LoadScene` 已在 `example/Geometry/CMakeLists.txt:23-24` 恢复。
3. **阶段 5 收口**（§6.5）：`addr_mesh_draw_params` 双源收敛——唯一仍在的数据面待办。
4. ~~MeshDrawParams 88B→112B 注释残留~~——**未做**：`ShaderBufferSources.h:10/63` 注释仍写 88B（纯注释，随下次改动修）。

---

## 8. 已拍板决策清单

1. 一个 `GlobalSSBOType` 只会有一个材质 SSBO（Arena 终身不重建）。
2. 材质字段专用类型另立枚举；纹理引用已独立成 `MaterialTextureReferencePool`（per-definition）。
3. 任意材质可访问所有材质字段 SSBO（固定配表：`PBRSurface` → `global_addresses.addr_pbr_surface` 等）。
4. BDA 分层：类型池基址 + MeshDrawParams 基址 → UBO；`mtl_data_addrs`/纹理引用池/L2W 基址 → push constants（L2W 暂 push）。**现状：mesh_draw_params 在 UBO 与 push 双源，收敛为待办（§6.5）。**
5. MeshDrawParams 池永久固定（预分配上限，**现为 16384 行**、`reserve_rows=1`；只增不减）。
6. 4-ID draw buffer 做 SSBO：现 ReBAR CPU 写，未来 CS 写（CS 写已在示例打通）。
7. L2W 沿用 static/dynamic 两段，**动态段 × `HGL_L2W_RING_FRAMES` 环形复用**（`graph::RingLayout`，`inc/hgl/vk/buffer/RingLayout.h:15-47`；`TransformAssignmentBuffer.h:55/70`）。
8. 材质字段类型池由 `GlobalSSBOBufferRegistry` 管理，默认 1024 行，超限 fail-fast。
9. `CameraInfo` 亦进全局池（64 行），由 `RootAddresses::camera_id` 索引取行（`scene_ubo.glsl:123`）。
10. 描述符终态两集：`Scene(0)`（6 个 UBO，`SceneBinding`）+ `Bindless(1)`；两集均带 `VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT`，**无 per-material 集**，全仓 `vkCmdBindDescriptorSets`/`vkCmdBindVertexBuffers`/`vkCmdBindIndexBuffer` 0 命中（改由 `vkCmdBindDescriptorBuffersEXT` + `vkCmdSetDescriptorBufferOffsetsEXT`）。

---

## 9. 风险与注意事项

1. **全局行分配器**：行号分配/释放，池 buffer 永不重建；释放复用前确认无 in-flight 引用。现由 `ActiveRowPool` 统一承载 `ReleaseDeferred`/`CollectRecyclable`（retire epoch）——各行池/纹理池不再各写一套。
2. **grow 语义**：类型池固定容量不扩容（fail-fast）；未来扩容则基址变 → 需刷新 `GlobalAddresses` UBO（帧边界执行）。
3. **`material_id` 层级**：两级（`material_id` → `MaterialInstanceAddresses` 行 → 类型池 `payload_index`），字段布局已由 8B 行 + offset 断言钉死。
4. **`transform_id` vs `addr_transform_id`**：实例级 L2W 行号 vs 顶点级 id 流——两者并存，未合并，改动前先澄清（§6.2 待澄清②）。
5. **ReBAR → CS 写**：4-ID SSBO 现 CPU 写；CS 写需同步机制（示例已跑通，引擎侧接入需评估）。
6. **L2W 归属**：暂 push；ring 帧数变化会影响池行号布局（`RingLayout::TotalRows`）。
7. **纹理引用池**：per-definition 建，容量 TOML `texture_configurations.max_count` 默认 1024，行 0 保留；回收走 retire epoch（延迟 3）。
8. **改名遗留**：文件名仍带旧前缀（`inc/hgl/graph/ssbo/MaterialSSBOLayout.h`、`MaterialDataRows.h`），内容已是 `Global*` 语义——纯命名，改动时勿被文件名误导。

---

## 10. 现状关键文件清单（供拆小计划定位）

| 层 | 文件 | 关键符号/行 |
|---|---|---|
| 全局类型枚举 | `inc/hgl/graph/ssbo/SSBOTypes.h` | `GlobalSSBOType`(:14-25)、`mtl::SSBOType`(:31-41)、`MakeRecipeSSBOId`/`MakeECSSSBOId`(:89/94)、`SSBOAddress`(:115) |
| 类型工具/绑定 | `inc/hgl/graph/ssbo/GlobalSSBOTypes.h` | `GetGlobalSSBOTypeName`(:16)、`GetGlobalSSBOTypeStructStride`(:45)、`GlobalSSBOBinding`(:64)、`GlobalSSBOConfig`(:78) |
| 行结构 | `inc/hgl/graph/ssbo/MaterialDataRows.h` | `PBRSurfaceRow`(32B)/`EmissiveSurfaceRow`(16B)/`TransmissionSurfaceRow`(16B)，断言(:40-45) |
| 布局/映射 | `inc/hgl/graph/ssbo/MaterialSSBOLayout.h` | `GetGlobalSSBOStructName`(:21)、`GetGlobalSSBORowName`(:41)、`GetGlobalSSBOBufferName`(:54)、`GetGlobalSSBOStructGLSL`(:65)、`GlobalRowTypeTraits`(:36-39) |
| 全局池 | `inc/hgl/graph/module/GlobalSSBOBufferRegistry.h` + `src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp` | `GlobalSSBODataAccessor`(:31)、`GlobalRowBufferInfo`(:63)、`Acquire/Write/ReleaseID`(:135-163)、`GetAccessor<T>`(:201)、`TryGetRowBuffer`(:220)、容量表(:13-20)、`InitializeGlobalAddressesUBO`(:85) |
| 池转发头 | `inc/hgl/graph/module/MeshDrawParamsPool.h` | 4 行，只 include 上者 |
| 通用域注册 | `inc/hgl/graph/module/SSBOBufferRegistry.h` | `SSBOBufferBinding`(:16)、`domain_map`(:29)、纹理引用池(:31/48-52)、`RegisterBuffer`(:60)、`GetNullRowAddress`(:113) |
| 纹理引用池 | `inc/hgl/graph/module/MaterialTextureReferencePool.h` + `.cpp` | `Acquire/Write/Retire/CollectRetired`(:102-110)、`MakePoolKey`(:75)、`row_generations`(:61)、retire 延迟(:15) |
| 纹理引用结构 | `inc/hgl/mtl/MaterialRecipe.h` | `MaterialTextureReference`(:182)、`MaterialTextureReferenceLayout`(:207)、`BuildMaterialTextureReferenceLayout`(:415)、`DefaultMaterialTextureConfigurationCapacity`(:68)、`MaterialTextureSamplingOptions`(:107)、`MaterialDefinition.material_private_data`(:299) |
| 行租约 | `inc/hgl/vk/buffer/ActiveRowPool.h` / `ActiveRowLease.h` / `ActiveRowView.h` | `Acquire`(:92)、`ReleaseDeferred`(:102)、`RowGPU`(:134)、`RowCPU`(:127)、`CommitRow`(:142) |
| UBO | `inc/hgl/graph/ubo/GlobalAddresses.h` | `struct GlobalAddresses`(:14-25) |
| MeshDrawParams | `inc/hgl/graph/ShaderBufferSources.h` | `HGL_MESH_DRAW_PARAMS_FIELD_LIST`(:16-33)、**112B** 断言(:85/:88)、`MeshDrawCommand`(:93-127)、`MaterialInstanceAddresses`(:132-140)、`DrawItem4ID`(:148-186)、`GeometryAABB`(:188-198) |
| RootAddresses | 同上 | `HGL_ROOT_ADDRESSES_FIELD_LIST`(:206-216)、**72B** 断言(:246-264) |
| push 下发 | `inc/hgl/graph/RootAddressPush.h` | `PushRootAddresses`(:28) |
| 描述符集 | `inc/hgl/common/DescriptorSetTypeDef.h` | `SceneBinding`(:12-22) + ABI `static_assert`(:26-30)、`DescriptorSetType`(:40-51)、宏表(:100-114) |
| Scene 集布局 | `src/Vulkan/VKGlobalSceneUBOSet.cpp` | binding 表(:29-70) |
| GLSL UBO | `ShaderLibrary/ubo/scene_ubo.glsl` | `GlobalAddressesInfo`(:88-97)、`camera` 宏(:123) |
| 4-ID 描述符 | `inc/hgl/graph/render/RenderItemDescriptor.h` | `RenderItemDescriptor`(:19-44) |
| 4-ID 池 | `inc/hgl/ecs/support/RenderItemDataStorage.h` / `DrawItemIDStorage.h` / `DrawItemCompaction.h` | `AllocateContiguous`(:69)、`Set4ID`(:91)、`SyncToGPU`(:120)、`CompactRenderItemHandles`(:64) |
| 4-ID GLSL | `ShaderLibrary/common/RenderItemResolve.glsl` | `ResolveRenderItemDirect`(:73)、`ResolveRenderItemIndexed`(:79)、`ResolveRenderItemAuto`(:112) |
| auth | `inc/hgl/ecs/components/PrimitiveComponent.h/.cpp` | `MaterialDataAuthoringResource`(:69)、`SetMaterialDataResource`(:227)、`SetMaterialTextureResource`(:218)、`SetMaterialTextureArrayLayer`(:225)、`Set4ID`(:257)、`render_item_descriptor`(:140) |
| recipe | `inc/hgl/mtl/MaterialRecipe.h` | 唯一 `GlobalSSBOBinding material_ssbo_binding`(:508) |
| 解析缓存 | `inc/hgl/ecs/components/MaterialComponent.h` | `cached_effective_recipe`(:53)；不再缓存 resolved binding/table |
| 收集/物化 | `src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp` | `MaterializeRecipeRowsForPrimitive`(:653)、`TryGetRowBuffer`(:750)、`BuildMaterialTextureReferenceLayout`(:839)、`AcquireMaterialTextureConfiguration`(:990)、`Set4ID`(:1418/1426) |
| 批/命令 | `src/ecs/support/PrimitiveBatchPipeline.cpp` | `EnsureMeshDrawParams`(:591)、`WriteMeshDrawCommands`(:644)、`MaterialInstanceAddresses` 行写(:889-1000)、Meshlet 双轨分流 |
| 批状态 | `inc/hgl/ecs/core/MaterialBatch.h` | `mesh_draw_params_buffer`(:55)、`l2w_index_buffer`(:60)、`material_data_index_rows_buffer`(:65)、`texture_reference_base_addr`(:67)、`gpu_driven_override`/`uses_render_item_resolve`(:70-71) |
| 渲染器 | `src/ecs/support/PipelineMaterialRenderer.cpp` | `PushRootAddresses`(:173)、`GetTransformDataBuffer`(:168) |
| 发射 | `src/ShaderGen/compile/MaterialShaderEmitter.cpp` | `BuildMaterialSSBODeclarations`(:197)、`MTL_ROW`(:291-296)、`MTL_TEX`(:322-325)、`MaterialInstanceAddressesRef`(:544) |
| 发射（mesh） | `src/ShaderGen/meshgen/MeshTemplateEmitter.h` / `MeshShaderVaryingGen.h` / `MeshShaderHeaderGen.h` | `MeshDrawCommandsRef(pc_root.addr_mesh_draw_params)`(:269-271)、`fragDataIndexID`(:115/149/150/199)、8-bit storage(:55) |
| 几何 | `inc/hgl/graph/geo/VKGeometry.h` + `src/SceneGraph/VKGeometry.cpp` + `inc/hgl/graph/mesh/GeometryDataBuffer.h` | `Geometry::geometry_id`(:121)、`GetGeometryID`(:126)、`EnsureMeshDrawParams`(:130)、`RegisterMeshDrawParams`(实现 :167-174)、Meshlet 缓冲(:95-116)、`GeometryDataBuffer::geometry_id`(:15) |
| 变换 | `inc/hgl/ecs/support/TransformAssignmentBuffer.h` | static/dynamic 段(:39/73)、`RingLayout ring_layout`(:55)、`EnsureCapacity`(:76)、`WriteStaticDirtyIndices`(:79)、`WriteDynamicDirtyIndices`(:82) |
| ring | `inc/hgl/vk/buffer/RingLayout.h` | `RingLayout`(:15-47)、`HGL_L2W_RING_FRAMES` |
| sampler 运行时 | `ShaderLibrary/sampler.toml`、`inc/hgl/mtl/SamplerPreset.h`、`src/ShaderGen/glsl_module/SamplerPreset.cpp`、`src/SceneGraph/render/GraphicsContext.cpp`、`src/Vulkan/VKBindlessTextureManager.cpp` | `SamplerPresetLibrary`(:29)、`Load`(:85)、`GetCreateInfo`(:77)、注册入口(:96-118)、`RegisterSamplers`(:330)、`RebuildSampler`(:373) |
| 文本数据 | `src/ecs/support/text/TextRenderPipeline.cpp` | `MaterialInstanceAddresses` 单行表(:528-560)、`PushRootAddresses`(:290) |
| 示例 | `example/Basic/PBRSpheres.cpp` | `InitMaterialDataSSBO`(:272)、`GetAccessor<PBRSurfaceRow>`(:286)、`SetMaterialDataResource`(:501)、`SetMaterialTextureResource(..., Texture2DArray, "", row)`(:482/490) |
| 示例 | `example/Basic/ComputeFrustumCull.cpp` / `ComputeAsteroidBelt.cpp` | 4-ID CS 剔除 + 紧凑(:92-155/254)、`DrawItem4ID` 候选表、`MeshDrawCommand` 表(:476-488 / :779-791) |
| 示例 | `example/Basic/AutoMergeMaterialInstance.cpp` / `BasicLitMeshes.cpp` | 同型材质 accessor + 单一材质数据资源 |

---

## 11. 基线后的新进展（2026-09-24 核对）

本节记录本次「文档 ↔ 代码」逐条反证的结论。核对方法：`rg` 全仓符号命中（排除 `build/`）+ 逐文件读真源 + `git log` 追溯提交。

### 11.1 已完成断言：旧符号 0 残留（成立）
| 旧符号 | 命中 | 结论 |
|---|---|---|
| `MaterialSSBOBufferRegistry` / `MaterialSSBODataAccessor` | 0 / 0 | 已改名 `Global*` |
| `MaterialSSBOBinding` | 0 | 类型已改名 `GlobalSSBOBinding`（字段名 `material_ssbo_binding` 保留） |
| `GetMaterialDataAccessor` / `GetMaterialSSBOStructName` / `GetMaterialSSBOTypeStructStride` / `MaterialRowTypeTraits` | 0 | 已改名 `GetAccessor<T>()` / `GetGlobalSSBOStructName` / `GetGlobalSSBOTypeStructStride` / `GlobalRowTypeTraits` |
| `GlobalPoolAddresses` | 0 | 终名 `GlobalAddresses`（+ GLSL block `GlobalAddressesInfo`） |
| `AllocateArrayAccessor` / `AllocateSSBOId` / `next_ssbo_id` / `row_segments` / `RowSegmentInfo` | 0 | SSBO 分配链已删，分配上收 `BufferManager` |
| `payload_address` / `texture_reference_address` | 0 | 已改 `payload_index` / `texture_reference_index` |
| `tex_tail` / `TextureSlot` / `TextureLayerRow` / `TextureRectArraySurfaceRow` / `TextureLayer` | 0 | 纹理固定槽方案已删 |
| `free_rows` | 0 | 纹理池行栈已删，改用 `ActiveRowPool` |

### 11.2 已完成断言：新符号已存在（成立）
`GlobalSSBOBufferRegistry`(143/54)、`GlobalSSBODataAccessor`(48/33)、`GlobalSSBOBinding`、`GetGlobalSSBOTypeStructStride`、`GlobalRowTypeTraits`、`GlobalAddresses`、`GlobalAddressesInfo`、`RenderItemDescriptor`(58/10)、`RenderItemDataStorage`(78/15)、`ResolveRenderItemDirect`/`ResolveRenderItemIndexed`、`PushRootAddresses`(11/8)、`HGL_ROOT_ADDRESSES_FIELD_LIST`、`bindless_textures`(39/20)、`SceneBinding`(82/16)、`DrawItem4ID`、`GeometryAABB`、`CompactRenderItemHandles`。

### 11.3 与旧文不符、已就地更正处
1. **基线日期与状态**：旧文基线 2026-09-18 且把阶段 1/3/6 标为完成、2/4/5 标为未做。实际 2026-09-19～23 已把 **阶段 2、4 完成**、**阶段 5 部分完成**、阶段 1/3/6 改名收口（`e9f5ec8bd`）。→ 已改 §0 头注、§5、§6。
2. **`MaterialInstanceAddresses` 16B 双地址 → 8B 双 index**（旧文 §2.3/§4 仍写 16B 双地址）。→ 已改 §2.4/§4。
3. **`RootAddresses` 56B（7×uint64）→ 72B（8×uint64 + 2×uint32）**，新增 `addr_texture_references`/`camera_id`/`_pad_camera`。→ 已改 §2.6/§3。
4. **`MaterialInstanceAddresses` 行写入点**从 `PrimitiveBatchPipeline.cpp:762/784/845` 漂移到 :889-1000；`MTL_ROW`/`MTL_TEX` 发射点从 `:167-169`/`:194-196` 漂移到 :291-296/:322-325；`MaterialInstanceAddressesRef` 从 :415 漂移到 :544。→ 已改 §2.4/§10。
5. **`MaterialTextureReference*` 行号漂移**：`MaterialTextureReference` :194→:182、`MaterialTextureReferenceLayout` :218→:207、`BuildMaterialTextureReferenceLayout` :447→:415、`DefaultMaterialTextureConfigurationCapacity` :80→:68、`MakePoolKey` :86→:75。→ 已改 §2.5/§10。
6. **纹理池内部机制**：旧文「自研 `free_rows` 栈」已删，改为 `ActiveRowPool` + `ActiveRowView` + `row_generations`。→ 已改 §2.5/§9。
7. **`SSBOBufferRegistry` 分配 API**旧文描述的 `domain_map`/`row_segments`/`next_ssbo_id`/`AllocateSSBOId`/`AllocateArrayAccessor` 组合已不成立（后四者删除）。→ 重写 §2.8。
8. **`GetMaterialSSBOTypeStructStride`「在 SSBOTypes.h」**实际在 `GlobalSSBOTypes.h:45`，并新增 `CameraInfo=656`。→ 已改 §2.3。
9. **MeshDrawParams 池容量**旧文标为待澄清「同 1024？」→ 现表驱动为 **16384 行**。→ 已改 §6.2/§8。
10. **sampler 运行时创建**旧文列为开放待办 → 已接入（`sampler.toml` → `SamplerPresetLibrary` → `RegisterSamplers`）。→ 已划掉 §7.4-1。
11. **`TransformDataBuffer`** 旧文作为独立概念 → 现为 `TransformAssignmentBuffer::GetTransformDataBuffer()` + `RingLayout` 环形布局。→ 已改 §7.2/§8-7。
12. **新增数据面**：`MeshDrawCommand`(8B)、`RenderItemDescriptor`/`RenderItemDataStorage`/`DrawItemIDStorage`/`DrawItemCompaction`、`GeometryAABB`、`GlobalSSBOType::CameraInfo`。→ 新增 §2.2/§2.10。
13. **旧文 §7.3「基线后的新进展（2026-09-18）」commit 表**已并入本文各节；2026-09-19～23 的提交按影响面重列为 §11.4（不再保留旧 5 行标题式补丁痕迹）。

### 11.4 基线后的提交（数据面相关）
| 提交 | 日期 | 内容 | 对本文的影响 |
|---|---|---|---|
| `22f47cf8e` | 09-19 | 全局池与 `SceneBinding`：实现 `MeshDrawParamsPool`，经 Set 0 binding 4 `GlobalAddressesInfo` UBO 传池 BDA | 阶段 2 + 阶段 5 起点（§2.6/§6.2/§6.5） |
| `3ea719317` | 09-19 | `GlobalSSBOBufferRegistry` 统一抽象与合并重构 | 阶段 1 收口（§2.7/§6.1） |
| `a193aeaf2` | 09-19 | `MaterialInstanceAddresses` 16B 双地址 → 8B 双 index | 阶段 3（§2.4/§6.3） |
| `cc97be792` | 09-19 | 描述符池与显存同步器基础设施 | 阶段 4 底座（§2.10） |
| `31436ece7` | 09-19 | ECS 组件接入与 4-ID 自动化登记 | 阶段 4（§2.10/§6.4） |
| `a306d9c35` | 09-19 | `InstancedPrimitiveComponent` 架构/实现/管线集成 + `ComputeAsteroidBelt` 迁移 | 连号直通分配（§2.10） |
| `226a592ea` | 09-19 | CPU 合批管线精简化——二级索引与连号直通折叠 | `DrawItemCompaction`（§2.10） |
| `e8e45baeb` | 09-20 | `AllocateContiguousInstances` / `SetAllInstances4ID` | 4-ID 实例化（§2.10） |
| `9a98eec2d` | 09-20 | 移除合批阶段动态 `EnsureMeshDrawParams` 补录；行表写入优先从 `RenderItemDataStorage` 解析 4-ID | 阶段 2/4 验收（§6.2/§6.4） |
| `e9f5ec8bd` | 09-21 | `MaterialSSBO` 系列统一并入 `GlobalSSBO` 系列；删 `MeshDrawParamsPool`/`MaterialSSOBufferRegistry` 转发头 | 本文全部旧名替换（§11.1） |
| `8a91df315` | 09-21 | `~Geometry` 归还 `MeshDrawParams` 行号前检查行有效性 | 池生命周期（§2.9） |
| `fef116296` | 09-21 | 模块创建顺序：`GeometryManager` 后移到 `GlobalSSBOBufferRegistry` 之后 | 池析构顺序约束（§9-1） |
| `c7430f92f` | 09-23 | `CameraInfo` 全局 SSBO 化与 PushConstants 索引 | 新增 `GlobalSSBOType::CameraInfo` + `camera_id`（§2.6/§8-9） |
| `554069a43` / `6c7b6344a` / `2987e5bfa` / `d07b85526` | 09-23 | 硬件 PCF、CSM 滚动缓存、环形阴影、RenderPass 增量条带 | 触及 Scene/Shadow UBO 与 RenderPass，不改 4-ID 数据面 |

### 11.5 未能核实（未改动，留待后续会话确认）
1. **阶段 5 的收口范围**：`addr_mesh_draw_params` 是否计划从 `RootAddresses` 删除（现为 UBO/push 双源），未见提交或文档决策记录；只能确认「现状双源」。
2. **`addr_transform_id`（顶点级 id 流）**是否已/需池化——未见该字段语义变更证据。
3. **Meshlet 三地址**是否需与顶点流地址区分规则——同在 112B 行内，无额外机制可查。
4. **`geometry_id` 粒度**：现为 per `Geometry`（单槽 + `EnsureMeshDrawParams` 短路）；是否还需 per 运行时实例 ID，无证据。
5. **CS 写 4-ID 是否接入引擎 ECS 主路径**：仅确认示例 `ComputeFrustumCull.cpp` 打通；引擎侧 `gpu_driven_override`/`uses_render_item_resolve`（`MaterialBatch.h:70-71`）的实际启用点未逐一核实。
6. **per-材质 `MaterialTextureSamplingOptions`** 运行时接入状态：仅确认其参与 layout hash 且注释标「not connected at stage 1」，未找到运行时消费点。
7. **`ActiveRowPool` 内部容器细节**（行号空闲表结构）未逐一核实，只确认对外 `Acquire/Release/ReleaseDeferred/RowCPU/RowGPU/CommitRow`。
8. **文件名与内容的旧前缀错配**（`MaterialSSBOLayout.h`/`MaterialDataRows.h` 内容已是 `Global*`）是否为有意保留，未见决策记录。
9. **`pbr_surface` 21 处命中**中 18 处为文档/`ShaderLibrary` 材质源模块名（`material/pbr_surface_source.glsl`），与 C++ 符号无关——未逐一确认每处语义。
10. **废弃时间点**：本次只做「当前 0 命中」判定，未追溯每个旧符号的具体删除提交。
