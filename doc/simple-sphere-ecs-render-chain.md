# SimpleSphere 到 vkCmd 的完整渲染链与数据流（技术文档）

> 基线：**2026-09-24，分支 `CSM`**——描述符机制退役后的 **BDA 终态**：4-ID 全局图元描述符池 + 全局 SSBO 行池 + `pc_root`/`buffer_reference` 寻址，mesh shader（`vkCmdDrawMeshTasksIndirectEXT`）是唯一顶点路径。
> 入口示例：`example/Basic/SimpleSphere.cpp`（264 行；私有缓冲几何 + `Lit` PBR 材质 + 单球体 ECS 实体 + `ViewModel` 相机）。
> 目的：以单个最小示例为剖面，记录「作者侧 API → 每帧 CPU 物化 → 一条 vkCmd 提交 → shader BDA 取数」的完整链路与行号证据。全部 path:line 在本基线核实；**符号名是稳定锚点，行号随重构漂移**，引用时按 `file:line` + 符号名双锚。
> 本文只描述现状，不含改造建议。

---

## 1. 总览：数据分三层落地

| 层 | 时机 | 干什么 | 产物 |
|---|---|---|---|
| ① 作者侧 | `Init()` 一次 | 申请全局材质行、建几何（几何自带全局 `MeshDrawParams` 行）、定材质定义、装纹理、建 4-ID 槽位 | 行号 `data_index`、`geometry_id`（全局几何池行号）、bindless 句柄、`RenderItemHandle` |
| ② 每帧 CPU 物化 | `RenderCollect` → `RenderFrameSync` 相位（cmd 已开、pass 外） | 行号→GPU 地址、建 `MeshDrawCommand` 表、建 `MaterialInstanceAddresses` 行表、上传脏段、挂场景 UBO | `MeshDrawCommand` 行、`MaterialInstanceAddresses` 行、mesh 间接命令、`GlobalAddressesInfo` 的 4-ID 表地址 |
| ③ GPU 侧 | 一条 `vkCmdDrawMeshTasksIndirectEXT`（multi-draw） | mesh shader 靠 `pc_root` + `gl_DrawID` + `gl_InstanceIndex` 直读全部数据 | 三角形 |

关键事实（终态，可复核）：

- **没有任何 per-draw / per-object / per-material 描述符集**，没有 `vkCmdBindVertexBuffers` / `vkCmdBindIndexBuffer`，也没有 vertex input state。
- 描述符集只剩 **2 个**（`inc/hgl/common/DescriptorSetTypeDef.h:52-64`）：**Scene(0)**（6 个 UBO：0=Camera / 1=Sky / 2=Viewport / 3=ColorPalette / 4=GlobalAddresses / 5=Shadow，`inc/hgl/common/DescriptorSetTypeDef.h:12-24`）与 **Bindless(1)**（全局纹理数组，`ShaderLibrary/common/bindless_textures.glsl:34-38`）。两集由 `GraphicsContext::BindGlobalDescriptorSets` 做设备级绑定（`inc/hgl/graph/core/GraphicsContext.h:140`），每 cmd 首绑一次全局复用。
- 行表、顶点流、材质行全部 `buffer_reference`（BDA）解引用；地址只有两个载体（见 §6）。

---

## 2. 入口 → 帧循环

```
os_main                                         example/Basic/SimpleSphere.cpp:261
└ RunFramework<SimpleSphereApp>(...)             inc/hgl/framework/WorkManager.h:66
  └ AppFramework::Init → RegisterDefaultEcsSystems   src/Work/AppFramework.cpp:251
  └ WorkManager::Run 帧循环                       src/Work/WorkManager.cpp:74
    ├ RunFrame → Tick(wo)                        src/Work/WorkManager.cpp:66 / :14
    │   └ WorkObject::Tick（球体自转）             example/Basic/SimpleSphere.cpp:248-258
    └ Render(wo)                                 src/Work/WorkManager.cpp:34
      └ ECSContext::Render(dt, pre_render)       src/ecs/core/Context.cpp:598
        ├ 只在 scene_structure_dirty 时 GatherSceneStats（结构稳定则零开销）
        │   命中 hash 变化才重建图                 src/ecs/core/Context.cpp:602-615
        └ ECSContext::Render(dt, graph, pre_render)   src/ecs/core/RenderGraph.cpp:162
          ├ BeginManagedRenderFrame(dt)              src/ecs/core/Context.cpp:331
          ├ ExecuteRenderGraphPasses(graph, dt, pre_render)   src/ecs/core/RenderGraph.cpp:91
          │   └ pre_render(dt) → WorkObject::OnRenderPass    inc/hgl/framework/WorkObject.h:118
          └ EndManagedRenderFrame(dt)                src/ecs/core/Context.cpp:383
```

单帧骨架（`BeginManagedRenderFrame`，`src/ecs/core/Context.cpp:331-381`）：

```
AcquireSwapchainImage(dt)                    Context.cpp:342 → 相位 RenderSwapchainNextImage
→ RenderPreBeginFrame(dt)                    Context.cpp:350（相位 RenderPreBeginFrame，cmd 未开）
→ SyncRenderTargetViewport()                 Context.cpp:351
→ TextureManager::UpdateUploadQueue(bindless) Context.cpp:353-359
→ render_core->BeginFrame()                   Context.cpp:362（从帧初始 RT 取 cmd buffer）
→ SetCurrentRenderCmd(cmd)                    Context.cpp:368
→ PrepareRenderPassSetup(swapchain_index, dt) Context.cpp:369 / :719-732
    SetFrameIndex → RenderCollect → RenderBatch
    → RenderBufferCommit → RenderBufferUpload → RenderFrameSync
→ render_core->BeginRenderPass(options)       Context.cpp:372 / RenderSystemCore.cpp:79
    → RenderCmdBuffer::BeginRendering（Dynamic Rendering）
      src/Vulkan/VKCommandBufferRender.cpp:36 → vkCmdBeginRendering :192
      附件 = swapchain 图，clear 值必须在此之前写入
→ pass 内的 Render()（相位 RenderDrawSubmit / RenderDebug）真正发 draw
→ render_core->EndFrame() → EndRenderingPresent（color → PRESENT_SRC 布局转换）
                                              RenderSystemCore.cpp:104 / :123
→ SubmitFrameToRenderTarget（相位 RenderSubmit → RT::Submit）
                                              Context.cpp:394 / :741
```

建图**只有一条路径**（`CreateDefaultLinearGraph` 已不存在）：`CreateAdaptiveRenderGraph`（`inc/hgl/ecs/core/RenderGraph.h:145`、`src/ecs/core/RenderGraph.cpp:217-262`）先按本世界 `SceneStats.active_render_groups` 逐组调 `SetElementTypeSystemsEnabled(group, enabled)`，命中的组各生成一段 pass（`RenderGraph.cpp:232-253`），未命中的组当场关掉该组全部系统。示例场景只有 `PrimitiveComponent` → 只有 `"Primitive"` 组有组件计数 → 只有 1 个 pass。

每帧系统序（按相位 + 注册先后推定，排序键 `(phase, insertion_order)` 见 `src/ecs/core/Context.cpp:879` 的 `SortSystemList`；本示例未实跑取日志）：

```
TickInput              InputSystem
TickTransform          TransformSystem → VisibilitySystem
TickCamera             CameraSystem

RenderPreBeginFrame      EnvironmentSystem → ColorPaletteSystem → RenderTargetSystem
RenderSwapchainNextImage SwapchainNextImageSystem
RenderCollect            RenderPrimitiveCollectSystem → PrimitiveCullSystem → PrimitiveSortSystem
RenderBatch              PrimitiveBuildSystem
RenderBufferCommit       ViewUBOCommitSystem
RenderBufferUpload       RenderBufferUploadSystem
RenderFrameSync          RenderSceneUBOSystem
RenderDrawSubmit         PrimitiveRenderSystem       ← 唯一发出 vkCmdDraw* 的系统
RenderDebug              PrimitiveOverlayRenderSystem
RenderSubmit             SwapchainSubmitSystem
```

相位注册实据：`src/ecs/core/DefaultSystems.cpp:165-207`（核心系统，按 environment→color_palette→render_target 顺序注册）+ `src/ecs/core/DefaultSystems.cpp:64-103`（`InstallPrimitiveGroup`：先 `RenderPrimitiveCollectSystem`，再 Cull/Sort/Build/Render/OverlayRender）；顺序推导器 `EnsureSystemGroupsRegistered` 取 `min(startPhase)/max(phase)` 作为组区间（`src/ecs/core/RenderGraph.cpp:21-87`）。

---

## 3. Init 阶段：数据在 CPU 侧怎么落地

### 3.1 材质参数行 → 全局行池租约（行号就是身份）

- 申请：`GetManager<GlobalSSBOBufferRegistry>()->GetAccessor<graph::ssbo::PBRSurfaceRow>()`（`example/Basic/SimpleSphere.cpp:90-96`，`GetAccessor` 模板见 `inc/hgl/graph/module/GlobalSSBOBufferRegistry.h:202-208`）
- 行结构真源：`graph::ssbo::PBRSurfaceRow` = `Color4f base_color + metallic/roughness/normal_scale/fresnel`，**32B**（`inc/hgl/graph/ssbo/MaterialDataRows.h:20-27`，`static_assert(sizeof(PBRSurfaceRow)==32)` 见 :43）
- 池在创建期按 `GlobalSSBOType` 枚举遍历建好：`ActiveRowPool pools[GlobalSSBOTypeCount]`（`GlobalSSBOBufferRegistry.h:88`），容量来自表驱动配置 `kGlobalSSBOConfigs`（`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp:13-20`）：
  - `MeshDrawParams` 16384 行 × 112B、`PBRSurface` **1024 行 × 32B**、`EmissiveSurface` 1024 × 16B、`TransmissionSurface` 1024 × 16B、`CameraInfo` 64 行；**每种池都 `reserve_rows = 1`（行 0 预留零行，不参与分配）**
  - 池一旦建好容量固定、超限 fail-fast（`src/Vulkan/buffer/ActiveRowPool.cpp:120` `Acquire`）
- 池本体 = `ActiveRowPool::Create`（`src/Vulkan/buffer/ActiveRowPool.cpp:13`）：`CreateArenaBuffer`（HOST_VISIBLE 整块持久映射 + `DEVICE_ADDRESS`）→ `GetBufferDeviceAddressAligned16`（纹理引出行要求 16B 对齐）→ 整块 `memset 0` + `MarkDirty`
- 写入：`acc.Write(row)`（`inc/hgl/vk/buffer/ActiveRowLease.h:157-166`）→ `ActiveRowView::WriteAs`（`inc/hgl/vk/buffer/ActiveRowView.h:174-188`）→ `WriteAt(0,…)` + `CommitByID`（:82）→ `ActiveRowPool::CommitRow`（`src/Vulkan/buffer/ActiveRowPool.cpp:191-203`）= 按行 `MarkDirty(id*row_bytes, row_bytes)`
- 返回 `GetGlobalSSBOBinding() = {ssbo_type, ssbo_id, data_index}`（`GlobalSSBOBufferRegistry.h:57-60`），喂两处：recipe 的 `material_ssbo_binding`（`SimpleSphere.cpp:118`）与组件的 authoring 资源（`SimpleSphere.cpp:189-191`）。**`data_index` 即池内行号 = shader 侧 `material_id`**。
- 池自身闭环管理 Set0/binding4 的 `GlobalAddressesInfo` UBO：`GlobalSSBOBufferRegistry::InitializeGlobalAddressesUBO`（`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp:85`，由 `InitializePools` :159 调用）。

### 3.2 recipe 与资产

`MaterialRecipe{ recipe_name, mtl_def_id="Lit", render_state_overrides.pipeline_config=MakeSolid3DConfig(), material_ssbo_binding }`（`SimpleSphere.cpp:115-119`）→ `PrimitiveAsset(geometry, &sphere_recipe, PrimitiveType::Triangles)`（:182）→ `PrimitiveComponent::SetPrimitiveAsset / SetMaterialTextureResource / SetMaterialDataResource`（:183-191）。
从 1024 个纹理池同理走 `ActiveRowPool`；CPU 侧 `ActiveRowPool::RowCPU` / `RowGPU` 给出同一行的两个视图（`inc/hgl/vk/buffer/ActiveRowPool.h:127-139`）。

### 3.3 几何 → 全局 `MeshDrawParams` 行（`geometry_id` 就是行号）

- `GeometryCreater geometry_creater(device, CreateStandardGeometryVertexFormat())`（3 流：Position `VF_V3F` / TexCoord `VF_V2HF` / Normal `VF_V2UN8`，`SimpleSphere.cpp:46-54`、:153-155）→ `CreateSphere(pc, 64)`（:157）→ `GeometryCreater::Create()`（`src/SceneGraph/geo/GeometryCreater.cpp:194`）内建 VAB（带 `SHADER_DEVICE_ADDRESS` usage）+ IBO；`geometry_manager->Add(...)`（`SimpleSphere.cpp:161`）
- **创建期即注册**：`GeometryCreater::Create()` 内直接调 `geometry->RegisterMeshDrawParams(pool, dev)`（`src/SceneGraph/geo/GeometryCreater.cpp:213-214`）；自由函数 `CreateGeometry(...)` 同样在末尾注册（:288-296）。索引类型被强制 U32（:240-243，因为 mesh 侧按 `uint[]` 读索引表）。私有 `new Geometry(...)` 路径需自行 `EnsureMeshDrawParams`。
- `Geometry::RegisterMeshDrawParams`（`src/SceneGraph/VKGeometry.cpp:112-176`）向全局 `MeshDrawParams` 池取一行并写入 **112B** 行（`inc/hgl/graph/ShaderBufferSources.h:16-41` 是字段唯一真源）：
  - 头部 6×4B：`index_base / vertex_base / is_indexed / total_vertices / char_height / first_instance`（`first_instance` 此处恒写 0，:125）
  - 尾部 **11×uint64** BDA 地址：`addr_position / addr_uv / addr_ntb / addr_color / addr_luminance / addr_transform_id / addr_size / addr_index`（IBO）+ meshlet 三表（:138-165），布局断言 `sizeof == 112` 见 `ShaderBufferSources.h:85-88`
  - `geometry_id == 0` 时 `Acquire`（:167-171），否则 `Write(geometry_id, params)` 原地更新（:174）——**`geometry_id` 就是该几何的参数行行号**，0 视为「未注册」
- 运行时包装（每几何一份 CPU 侧视图）在首次解析时生成：`runtime_data_buffer = new GeometryDataBuffer(gvf.GetCount(), geometry->GetIBO(), geometry->GetVDM())` 后 `Update(geometry)` 按语义把每个 VAB 的 `VkBuffer` 填进 `vab_list/vab_semantic`（`src/ecs/components/PrimitiveComponent.cpp:103-179`、`src/SceneGraph/mesh/Primitive.cpp:115-155`）；`GeometryDataBuffer.geometry_id` 在 `Update` 里同步（`src/SceneGraph/mesh/Primitive.cpp:120`）
- 私有路径与 VDM 路径的差别只在 `geometry->GetVDM()!=nullptr`（共享池段 vs 每几何独立缓冲；`Geometry::GetVDM()` `inc/hgl/graph/geo/VKGeometry.h:91`）。`GeometryDrawRange` 的 `vertex_offset/first_index` 恒 0（私有）或段偏移（VDM），且这两个值在几何注册期已被烘进行的 `index_base/vertex_base`。
- **per-draw 的 `first_instance` 不由几何行携带**：几何行写的恒为 0，每帧由 `MeshDrawCommand` 行覆写（§4、§6）。

### 3.4 材质定义 → program → pipeline

- `ShaderLibrary/material/lit.material.toml`：`[vertex] requirements=["Position","UV0","Normal"]`（:17-19）；`[resources] ubos=["CameraInfo","SkyInfo"]`、`textures` = **6 个可选槽**（`base_color/roughness/metallic/occlusion/opacity_mask/normal`，`sampler="Sampler2DArray"`，`normal` 带 `channels=2`）、`samplers=["Trilinear","Linear","ShadowMap","ShadowPCF"]`（:21-31）
- ShaderGen 编译成 `program-<hash>`；`RenderPrimitiveCollectSystem::ResolveRuntimePipelineForPrimitive`（`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:610`）取到 program 后 `render_pass->CreatePipeline(program, effective_recipe)`，key = shader stages + 附件格式；**每个 RenderPass 各自持有一份解析结果**（`PrimitiveComponent::resolvedRuntimePipelineMap`，`inc/hgl/ecs/components/PrimitiveComponent.h:130`、:188-202）——同一世界渲到多个 RT（shadow map 等离屏 pass）时各 RT 用各自格式匹配的管线，互不驱逐。

### 3.5 纹理 → bindless 句柄 + 引用池

- `LoadTexture2D` 得到 `Texture2D*`（`SimpleSphere.cpp:121-131`）；`RenderSceneUBOSystem::RegisterTextureResource` 把资源登记进 `BindlessTextureManager` 并缓存 `resource_id → handle`（`src/ecs/systems/render/RenderSceneUBOSystem.cpp:250-270`），返回 **1-based 句柄**（0 = 无效）；材质化期通过 `RenderSceneUBOSystem::GetBindlessHandle`（:271-278）取回
- 按 (definition, layout) 建引用池：`MaterialTextureReferencePool::MakePoolKey`（`inc/hgl/graph/module/MaterialTextureReferencePool.h:75-91`）→ `Initialize`（:96）
  - 行宽 = `reference_count × sizeof(uvec2)`（`Lit` = 6×8 = **48B**）；**行 0 恒为零行**（`ActiveRowPool` 的 `reserve_rows=1`，`MaterialTextureReferencePool.h:59` 注释明示）；`GetZeroRowAddress()`（:127）即 `material_texture_zero_row_gpu` 的来源
  - 创建日志：`[MaterialTextureReferencePool] created definition=%s references=%u row_stride=%u capacity=%u bytes=%llu`（`src/SceneGraph/module/MaterialTextureReferencePool.cpp:82-89`），`bytes = row_stride × 物理行数`
- 材质化期：`AcquireMaterialTextureConfiguration`（`MaterialTextureReferencePool.cpp:126`）+ `WriteMaterialTextureConfiguration` 写引用行，行内容 = 每个纹理声明一个 `(handle, array_layer)`；复用条件 = 同 pool_key + 同 reference_count/row_stride + 内容 hash 相同
- 退役语义：`Retire`（:201）+ 常量 `MaterialTextureConfigurationRetireEpochDelay = 3u`（`MaterialTextureReferencePool.h:15`）+ `CollectRetired`（:213）

### 3.6 4-ID 描述符槽位（一级表）

- 每个可渲染对象在全局一级表占一个 **16B `RenderItemDescriptor` = `{transform_id, geometry_id, material_id, texture_id}`**（`inc/hgl/graph/render/RenderItemDescriptor.h:19-44`，`static_assert(sizeof==16)` :41），由 `PrimitiveComponent::EnsureRenderItemStorageAllocated` / `GetRenderItemHandle` 惰性分配（`src/ecs/components/PrimitiveComponent.cpp:601-620`），`OnAttach` 顺带保证已分配（:686-690）、`OnDetach` 释放（:692-701）
- 存储 = `RenderItemDataStorage`（CPU 连续数组 + 空闲槽 + 脏范围 + GPU 镜像，`inc/hgl/ecs/support/RenderItemDataStorage.h:29-130`）；二级索引表 = `DrawItemIDStorage`（离散 handle 的当帧线性排布，`inc/hgl/ecs/support/DrawItemIDStorage.h:25-90`）
- 两者的 GPU 地址经 `RenderSceneUBOSystem::ResolveGlobalAddressesUBO`（`src/ecs/systems/render/RenderSceneUBOSystem.cpp:152-181`）写入 `GlobalAddresses` UBO 的 `addr_global_render_items` / `addr_draw_item_ids`（写者 `GlobalSSBOBufferRegistry::UpdateRenderItemAddresses`，`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp:128-143`，仅在变化时重写）

---

## 4. 每帧 CPU 物化（`RenderCollect` → `RenderFrameSync`）

`PrepareRenderPassSetup`（`src/ecs/core/Context.cpp:719-732`）严格按枚举序：

```
SetFrameIndex(frameIndex)                                   Context.cpp:726 → :1113
RunRenderPhaseUpdates(RenderCollect)                        Context.cpp:727
RunRenderPhaseUpdates(RenderBatch)                          Context.cpp:728
RenderBufferCommit(dt)  → RunRenderPhaseUpdates(...)         Context.cpp:695-701
RenderBufferUpload(dt)  → RunRenderPhaseUpdates(...)         Context.cpp:703-709
RenderFrameSync(dt)     → RunRenderPhaseUpdates(...)         Context.cpp:711-717
```

1. **`RenderCollect`**（`RenderPrimitiveCollectSystem::Update`，`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:1090-1449`）
   - `cache.BeginFrame()` 清 `renderItems`、清计数、调各 `MaterialBatch::Clear()`（保留批对象复用）（:1108）
   - 取 `VisibilityDataStorage` 做 O(1) 不可见跳过（:1110-1116）、取 `active_mobility_filter`（:1118）
   - **预扫描 + 物化 epoch**（:1141-1216）：判 `fast_path_holds`（program 未脏 + authored generation 未变 + recipe hash 有效，:1186-1191）与 `epoch_stale`（:1193-1195），得 `needs_work`（:1207-1209）；`any_material_work && any_possible_runtime_rows_visible` 才 `++materialize_epoch`（:1215-1216）
   - **主循环**（:1225-1439）过滤 `!IsVisible / !CanRender` / storage 不可见 / 无 owner / 无 transform / mobility filter / 无 recipe → 拿或建 `MaterialComponent` → 三条路径：
     - 全清帧快速路径（:1286-1311）：只跑 `ResolveMaterialProgramForPrimitive` + `EnsureRuntimeGeometryFromAsset` + `ResolveRuntimePipelineForPrimitive`，成功 `MarkValid()`，失败 `MarkFailed()`（下帧重试）
     - 需要工作帧（:1312-1372）：`MarkResourcesPending` → `PrepareActivePlanResources` → `MaterializeRecipeRowsForPrimitive` → `EnsureRuntimeGeometryFromAsset` → `ResolveRuntimePipelineForPrimitive` → `MarkValid()`；任一步失败 `InvalidateRecipeRuntime` + `MarkFailed`
   - **材质行地址物化**（`MaterializeRecipeRowsForPrimitive`，:653-1088）：校验 recipe 的 `material_ssbo_binding` 与 schema → 取 `graphics_context->GetGlobalSSBOBufferRegistry()`（:746）→ `TryGetRowBuffer(ssbo_id, info)`（:750）+ `IsActive(type, data_index)`（:762）+ `data_index < row_capacity`（:779-791）→ `material_row_gpu = info.gpu_base + data_index*info.row_bytes`（:801）、同时填 `material_row_cpu`；`material_comp->data_index_row = data_index`（:815）
   - **纹理引用行**：算配置 hash → 复用或 `Acquire`（:990）+ `Write`（:1004）→ `material_texture_row_gpu` / `material_texture_zero_row_gpu`（:1027-1034）；失败路径 `Retire` + 清零（:1010-1081）
   - **产出 4-ID**（:1377-1428）：`transform_id = transform->GetStorageHandle()`、`geometry_id`（取 `runtime_data_buffer->geometry_id`，为 0 时现场 `EnsureMeshDrawParams` 补注册，:1385-1402）、`material_id = data_index_row`、`texture_id = material_texture_configuration.row_index`；`PrimitiveComponent::Set4ID(...)`（:1418/:1426，Instanced 走 `SetAllInstances4ID`，:1414）
   - 生成 `PrimitiveRenderItem` / `InstancedPrimitiveRenderItem`，填 `distanceToCamera`、`UpdateWorldMatrix()`，push 进 `cache.renderItems`、`renderableCount++`（:1421-1438）
   - `PrimitiveCullSystem`（`src/ecs/support/primitive/PrimitiveCullSystem.cpp:10`）→ `PrimitiveRenderPipeline::RunCull` → `PrimitiveBatchPipeline::PerformFrustumCulling`（`src/ecs/support/PrimitiveBatchPipeline.cpp:153-186`，`TestFrustumWithWorldAABB/LocalAABB/BoundingSphere` :187-229）
   - `PrimitiveSortSystem`（`src/ecs/support/primitive/PrimitiveSortSystem.cpp:10`）→ `RunSorting → SortByDistance`（`PrimitiveBatchPipeline.cpp:231-243`）
2. **`RenderBatch`**（`PrimitiveBuildSystem`，`src/ecs/support/primitive/PrimitiveBuildSystem.cpp:10`）→ `RunBuild` = `PrepareFrame`（`PrimitiveBatchPipeline.cpp:89-118`，幂等守门）+ `RunTransformIndexing`（:130-145 → `AssignTransformIndices` :244-302）+ `RunBatching`（:147-151 → `BuildMaterialBatches` :1014-1088 + `FinalizeBatches` :1090-1102）
   - `FinalizeBatch`（:732-784）→ `SortBatchItems`（:786-831，静态段在前 + `Compare`，写 `item->index`）→ `EnsureBatchIndexRows`（:833-916，pow2 扩容 L2W 索引表与 `MaterialInstanceAddresses` 行表，**全表预填零**）→ `EnsureMeshDrawParams`（**:591-642**，pow2 扩容 `MeshDrawCommand` 表 + 整表 `memset 0`）→ `BuildBatches`（:366-589，同 `GeometryDataBuffer` + 同 `GeometryDrawRange` 的相邻项聚成 DrawBatch；4-ID 有效时走 `CompactRenderItemHandles` 连号折叠，`inc/hgl/ecs/support/DrawItemCompaction.h:64-70`）→ `WriteBatchIndexRows`（:918-1013）
   - `WriteMeshDrawCommands`（:644-730）：每个 DrawBatch 写一行 **`MeshDrawCommand`（8B = `geometry_id` + `first_instance`）**（:657-687），并写 mesh 命令 `{groupCountX, groupCountY=instance_count, groupCountZ=1}`（有 meshlet 用 `geometry->GetMeshletCount()`，否则 `CalcMeshGroupCount`，`inc/hgl/ecs/support/PipelineMaterialRenderer.h:44-51`；:712-723）；`gpu_driven_override` 的批次整段跳过（:646）
   - `WriteBatchIndexRows`（:918-1013）：每实例写 `l2w_index`（`item->transform_index`，:935-940）与 `MaterialInstanceAddresses{payload_index, texture_reference_index}`——优先取 `RenderItemDataStorage` 的 4-ID 描述符（:969-979），未注册则从 `MaterialComponent` 回退并顺便反推批次纹理引用池基址（:981-1005）
3. **`RenderBufferCommit`**（`ViewUBOCommitSystem::Update`，`src/ecs/systems/render/ViewUBOCommitSystem.cpp:17-36`）：`CameraSystem::CommitCameraUBO`（`src/ecs/systems/tick/CameraSystem.cpp:310-319`，**无条件全量写** view 三件套，不依赖脏标记）+ `RenderSceneUBOSystem::CommitViewportUBO`（`src/ecs/systems/render/RenderSceneUBOSystem.cpp:183`）+ `EnvironmentManager::CommitMaterialized`
4. **`RenderBufferUpload`**（`RenderBufferUploadSystem::Update`，`src/ecs/systems/render/RenderBufferUploadSystem.cpp:20-149`）：先 `RenderItemDataStorage::SyncToGPU` / `DrawItemIDStorage::SyncToGPU`（:47-56），再遍历 `device->GetGPUBufferRegistry()` 对脏 buffer `CopyToDevice`，最后一条 `MemoryBarrier2`（transfer → draw-indirect/vertex/index/VS/FS/CS）
5. **`RenderFrameSync`**（`RenderSceneUBOSystem::Update`，`RenderSceneUBOSystem.cpp:280`）→ `SyncBindingsForCurrentCommand`（:291）→ `ApplyResourceLayoutBindings`（:378-410）：把 camera/sky/viewport/global_addresses/shadow 挂进设备级 `VKGlobalSceneUBOSet`（一帧一次，binding 表见 `inc/hgl/vk/VKGlobalSceneUBOSet.h:19-25`）——**Scene 集的数据写入者，勿删**。`Render(graph::RenderCmdBuffer*, float)` 空实现（:285）。

---

## 5. 提交绘制（`RenderDrawSubmit` → vkCmd）

`PrimitiveRenderSystem::OnRender`（`src/ecs/support/primitive/PrimitiveRenderSystem.cpp:39-94`）：从 `cache.materialBatches` 收非空批 → 按 `MaterialBatch::key` 排序（:60-65）→ 跳过 `pipeline->GetOverlay()` 的批（:72-73，交给 Overlay 系统）→ `renderer->Render(cmdBuffer, batch->draw_batches, batch->draw_batches_count, batch->transform_buffer, batch, context->GetRenderContext(), context->GetActiveCameraID())`（:86-92）。

`PipelineMaterialRenderer::Render`（`src/ecs/support/PipelineMaterialRenderer.cpp:103-208`）对每个批次只做这几件事：

| 步 | 代码 | 内容 |
|---|---|---|
| 1 | :124 | `BindPipeline(pipeline)` —— pipeline 只含 shader 部分 |
| 2 | :127-128 | `ApplyPipelineState(pipeline->GetConfig())` —— EDS 1/2/3 全套 `vkCmdSet*`（cull/depth/blend/polygon/…） |
| 3 | :132-136 | `GraphicsContext::BindGlobalDescriptorSets(cmd, layout)` —— Scene(0)/Bindless(1) 每 cmd 首绑一次（`GraphicsContext` 内部守卫） |
| 4 | :159-187 | `graph::PushRootAddresses(...)` —— `pc_root`：mesh_draw_params / l2w / l2w_index / mtl_data_addrs 基址 + `texture_reference_base_addr` + 文本三表（nullptr）+ `camera_id` |
| 5 | :195-207 | 累积 DrawBatch 计数 → 批末 `ProcIndirectRender`（:37-74）：`DrawMeshTasksIndirect` 或 `DrawMeshTasksIndirectCount`（icb 有计数缓冲时）一条 multi-draw |

最终落点：

```
src/Vulkan/VKCommandBufferRender.cpp:416-424
  dev_attr->cmd_draw_mesh_tasks_indirect(cmd_buf, buffer, offset, drawCount, stride)
  → pfn vkCmdDrawMeshTasksIndirectEXT（设备创建时 vkGetDeviceProcAddr 加载）
  计数版：:426-433  dev_attr->cmd_draw_mesh_tasks_indirect_count(...)
  声明：inc/hgl/vk/VKCommandBuffer.h:258-263
```

一次 multi-draw 提交本批**全部** DrawBatch：命令序 = DrawBatch 序 = 参数行序；每条命令靠 `gl_DrawID` 查 `MeshDrawCommand` 行，再靠 `gl_InstanceIndex` 查 `l2w_index` / `MaterialInstanceAddresses` 行。`Draw`（:76-101）在 mesh 路径下只做计数累积（`++indirect_draw_count`），不再切任何 buffer/描述符。

---

## 6. GPU 侧：shader 怎么把数取回来

先说两个地址载体——**名字都叫 `addr_mesh_draw_params`，但语义完全不同**：

| 载体 | 内容 | 谁写 | 真源 |
|---|---|---|---|
| `GlobalAddressesInfo` UBO（Set0 / binding4） | 7×uint64 = **56B**：`addr_mesh_draw_params`（**全局几何参数池**，112B 行，按 `geometry_id` 索引）、`addr_pbr_surface`、`addr_emissive_surface`、`addr_transmission_surface`、`addr_global_render_items`、`addr_draw_item_ids`、`addr_camera_info`。启动写一次，终身不变（后两项仅在有变化时经 `UpdateRenderItemAddresses` 重写） | `GlobalSSBOBufferRegistry::InitializeGlobalAddressesUBO`（`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp:85`）/ `UpdateRenderItemAddresses`（:128） | `inc/hgl/graph/ubo/GlobalAddresses.h:14-25`；GLSL 声明 `ShaderLibrary/ubo/scene_ubo.glsl:88-97` |
| `RootAddresses` push constant（`pc_root`） | 8×uint64 + 2×uint32 = **72B**：`addr_mesh_draw_params`（**本批 `MeshDrawCommand` 表**，8B 行，按 `gl_DrawID` 索引）、`addr_l2w`、`addr_l2w_index`、`addr_mtl_data_addrs`、`addr_texture_references`、`addr_text_char_info/style/instance`、`camera_id` + pad | 每 MaterialBatch 渲染前一次 `graph::PushRootAddresses` | `inc/hgl/graph/ShaderBufferSources.h:206-264`（布局断言 :245-264）；`inc/hgl/graph/RootAddressPush.h:28-64`；调用点 `src/ecs/support/PipelineMaterialRenderer.cpp:173-186` |

取数表（左列 = 需要什么，右列 = 生成文本 / 源文件）：

| 需要什么 | 从哪取 |
|---|---|
| 本 draw 的几何 id + 首实例 | `geometry_id = MeshDrawCommandsRef(pc_root.addr_mesh_draw_params).cmds[gl_DrawID].geometry_id`；`first_instance` 同行（`src/ShaderGen/meshgen/MeshTemplateEmitter.h:269-271`；结构体与 ref 类型由 `src/ShaderGen/meshgen/MeshShaderVertexAdapter.h:61-74` 从 X 列表发射） |
| 本 draw 的几何参数行（112B） | `draw_params = MeshDrawParamsRef(global_addresses.addr_mesh_draw_params).rows[geometry_id]`，再用命令行的 `first_instance` 覆写（`MeshTemplateEmitter.h:270-271`；非 VertexPassthrough/LineQuad 模式退化为 `rows[gl_DrawID]`，:275） |
| 顶点号 | `MeshVertexIndex = group_base_vertex + vid`；索引几何再 `sbo_vertex_index.data[draw_params.index_base + MeshVertexIndex]`（`ShaderLibrary/mesh/vertex_passthrough.glsl.tmpl:16-18`） |
| Position | `VertexPositionRef(draw_params.addr_position).data[draw_params.vertex_base + VertexIndexID]`（`ShaderLibrary/vertex/s1_position_vec3.glsl:21,27`） |
| UV | `unpackHalf2x16(VertexUVPackedRef(draw_params.addr_uv).data[draw_params.vertex_base + VertexIndexID])`（`ShaderLibrary/vertex/s1_uv_rg16f.glsl:16,20`） |
| Normal | RG8 packed uint → octahedral 解码（`ShaderLibrary/vertex/s1_ntb_rg8.glsl:14,20-32`） |
| 世界变换 | `l2w` 宏 = `LocalToWorldDataRef(pc_root.addr_l2w)`（`ShaderLibrary/common/l2w_ssbo.glsl:21-26`），下标经 `pc_root.addr_l2w_index` 行表 |
| 每实例材质行 | `MTL_ROW(i) = PBRSurfaceRowRef(global_addresses.addr_pbr_surface + uint64(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[i].payload_index) * 32)`（发射于 `src/ShaderGen/compile/MaterialShaderEmitter.cpp:291-297`） |
| 每实例纹理引用 | `MTL_TEX(i) = MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64(...values[i].texture_reference_index) * 48)`（`MaterialShaderEmitter.cpp:322-325`；`MaterialTextureReferencesRef` 声明 :310-320） |
| 纹理采样 | `bindless_tex[nonuniformEXT(handle-1)]` + `bindless_samp[采样器常量]`；可选槽走 `SampleOptional`（句柄 0 → fallback）（`ShaderLibrary/common/bindless_textures.glsl:42-58,86-91`）；消费点 `ShaderLibrary/material/pbr_surface_source.glsl:26,43-50` |
| 相机 / 天空 / 视口 / 阴影 | Scene 集（set=0）的 CameraInfo / SkyInfo / ViewportInfo / ShadowInfo UBO（`ShaderLibrary/ubo/scene_ubo.glsl:61-121`）。**相机已 SSBO 化**：`camera` 宏 = `CameraInfoBufferRef(global_addresses.addr_camera_info).cameras[pc_root.camera_id]`（`scene_ubo.glsl:123`） |
| 4-ID 运行时解析（GPU-Driven 路径） | `ResolveRenderItemDirect(instance_index)` / `ResolveRenderItemIndexed(draw_id)`（`ShaderLibrary/common/RenderItemResolve.glsl:73-83`）；`RENDER_ITEM_INDEXED_FLAG = 0x80000000u` 由 `ResolveRenderItemAuto` 自动分流（:99-125）。`MaterialBatch::uses_render_item_resolve` 决定是否启用该路径（`inc/hgl/ecs/core/MaterialBatch.h:71`） |

约定（`inc/hgl/graph/ShaderBufferSources.h`）：`MeshDrawParams` 头部 6×4B 连续 + 11×8B 连续 = **112B**（断言 :62-88）；`MeshDrawCommand` = 2×uint32 = **8B**（断言 :121-127）；`MaterialInstanceAddresses` = 2×uint32 = **8B**（断言 :138-140）；`DrawItem4ID` = 4×uint32 = **16B**（断言 :178-186）；`RootAddresses` = 8×uint64 + 2×uint32 = **72B**（断言 :245-264）。所有 buffer 以 `SHADER_DEVICE_ADDRESS` usage 创建，地址取 `VulkanDevice::GetBufferDeviceAddressAligned16`（非 16B 对齐基址 fail-fast）。

---

## 7. 诊断日志与自洽性校验

当前代码里存在的相关日志格式串（行号为发射点）：

```
[MaterialTextureReferencePool] created definition=%s references=%u row_stride=%u capacity=%u bytes=%llu
                                              src/SceneGraph/module/MaterialTextureReferencePool.cpp:82-89
[DeferredResource] owner=%s program=%s planned_texture=%u planned_data=%u recipe_texture=%zu recipe_data=%zu
                                              src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:581-588
[ArenaTrace] materialize entry / ssbo_id=… data_index=… / translated: gpu=…
                                              RenderPrimitiveCollectSystem.cpp:684 / :735 / :806（ULRE_ARENA_DEBUG 置位时）
[IndirectMeshDraw] mesh indirect flush engaged: first=%d count=%u
                                              src/ecs/support/PipelineMaterialRenderer.cpp:47-48（仅首次）
[LineStats] total=… visible=… culled(vis=… frustum=… hzb=…)
                                              src/ecs/systems/render/LineStatsSystem.cpp:34-35
[SceneUBO] Scene UBO set not bound: camera=%p viewport=%p sky=%p shadow=%p
                                              src/ecs/systems/render/RenderSceneUBOSystem.cpp:404-408
[RenderGraph] Registered %zu system groups (total %zu)   src/ecs/core/RenderGraph.cpp:83-84
[RenderGraph]   active group: %s                          src/ecs/core/RenderGraph.cpp:211
```

由代码直接推出的自洽关系（不依赖某一次运行）：

- 行地址 = 池基址 + 行号 × 行距：`material_row_gpu = info.gpu_base + data_index*info.row_bytes`（`RenderPrimitiveCollectSystem.cpp:801`）；纹理侧 `RowGPU(id) = gpu_base + id*row_bytes`（`inc/hgl/vk/buffer/ActiveRowPool.h:134-139`）
- 纹理引用池行数比 `max_configuration_count` 多 1（预留零行），`GetZeroRowAddress()` 即 `GPUBase + 0`（`MaterialTextureReferencePool.h:59,127`）
- `payload_index` / `texture_reference_index` 两列是「写入点 ↔ 读取点」一一对应：`WriteBatchIndexRows`（`PrimitiveBatchPipeline.cpp:975-994`）写 → `MTL_ROW` / `MTL_TEX` 宏（`MaterialShaderEmitter.cpp:291-297,322-325`）读
- 未绑定纹理槽写 `(0,0)`，`SampleOptional` 以 `tex_ref.x == 0` 走 fallback（`bindless_textures.glsl:86-91`）

复现：`cmake --build build --config Debug --target SimpleSphere` → 设 `ULRE_ARENA_DEBUG=1` 起 `build/out/Windows_64_Debug/SimpleSphere.exe`（cwd = 仓库根，`res/` 在根）。

---

## 8. 改动这套链时的雷区（现状约束）

1. **身份 = 物理行号**：`data_index` / `geometry_id` / `texture_reference_index` 全是池内行号，不是「块内偏移」。全局池容量固定（`kGlobalSSBOConfigs`，`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp:13-20`），不扩容，超限 fail-fast（`ActiveRowPool::Acquire`，`src/Vulkan/buffer/ActiveRowPool.cpp:120-168`）。
2. **寻址只靠两个内建索引**：per-draw 用 `gl_DrawID`（`MeshDrawCommand` 表），per-instance 用 `gl_InstanceIndex`（`l2w_index` 行表 / `MaterialInstanceAddresses` 行表）。因此「命令序 = DrawBatch 序 = 行序」是硬约束，任何新增/合并 DrawBatch 的改动都要同步维护这三处行序。
3. **72B `pc_root` 每批 push 一次**，push 的是**表地址**而非 per-draw 数据——一次 multi-draw 内 N 条命令共享一份，不违反合批。任一渲染路径漏 push → shader 解引用 0 → 静默黑屏（无 VUID、无 GPU fault）。同理 `GlobalAddresses` UBO 的 7 个地址必须全非 0。
4. **纹理引用池 retire 延迟** `MaterialTextureConfigurationRetireEpochDelay = 3u`（`inc/hgl/graph/module/MaterialTextureReferencePool.h:15`）：改帧在飞数或加深提交管线必须同步加大该延迟，否则回收清零会落在仍被 GPU 读的行上（症状：静默丢纹理）。
5. **三个渲染路径（Primitive / Line / Text）都必须 `BindPipeline` 后紧跟 `ApplyPipelineState`**，且各自 push 自己消费的表（Text 走 `addr_text_char_*` 三表，不消费 l2w/材质行）。
6. **几何参数行必须在使用前注册**：`geometry_id == 0` 表示未注册，而 `MeshShaderVertexAdapter` 生成的 shader 会用 `rows[geometry_id]` 直接解引用——行 0 是预留零行（全 0 地址），不会崩但什么都不画。`GeometryCreater::Create()` 已自动注册；私有 `new Geometry(...)` 路径需自行 `EnsureMeshDrawParams`。
7. **`render_item_handle` 是 4-ID 一级表的唯一身份**：槽位在组件 `OnAttach` 时分配、`OnDetach` 时释放（`src/ecs/components/PrimitiveComponent.cpp:686-701`）；跨帧缓存裸 handle 会踩空槽。
8. **`first_instance` 有两个来源，不要混**：几何参数行里的 `first_instance` 恒 0（`src/SceneGraph/VKGeometry.cpp:125`），真正的 per-draw 值在 `MeshDrawCommand` 行里（`PrimitiveBatchPipeline.cpp:682`），shader 侧显式覆写（`MeshTemplateEmitter.h:271`）。

---

## 9. 基线后的新进展（以代码为准）

本节以现状为基线，逐条列本次校正涉及的「旧说法 → 现状 + 依据」。

| 旧说法 | 现状 | 依据 |
|---|---|---|
| `MaterialSSBOBufferRegistry` / `MaterialSSBODataAccessor` / `GetMaterialDataAccessor` / `GetMaterialSSBOBinding` / `IsMaterialDataIDActive` / `material_row_pools` / `DefaultMaterialDataElementCapacity` | 统一为 `GlobalSSBOBufferRegistry` + `GlobalSSBODataAccessor` + `ActiveRowPool pools[GlobalSSBOTypeCount]`；容量来自 `kGlobalSSBOConfigs` 表（PBRSurface 1024×32B / MeshDrawParams 16384×112B / CameraInfo 64）；行号活跃判定是 `IsActive(ssbo_type, id)`，绑定是 `GetGlobalSSBOBinding()`。旧名全仓 0 命中 | `inc/hgl/graph/module/GlobalSSBOBufferRegistry.h:31-61,88,147-168,220`；`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp:13-20,169`；`example/Basic/SimpleSphere.cpp:90-96,118,190` |
| 材质 SSBO 类型枚举 `MaterialSSBOType` | 并入 `graph::GlobalSSBOType`（MeshDrawParams / PBRSurface / EmissiveSurface / TransmissionSurface / CameraInfo） | `inc/hgl/graph/ssbo/SSBOTypes.h:14-25`（注释明写“原 mtl::MaterialSSBOType 已并入本枚举”） |
| 材质行 `data_index` 兼作地址 | `data_index` 是**行号**（`material_id`）；地址由 `material_row_gpu = gpu_base + data_index*row_bytes` 现场算出。返回结构体字段名从 `row_id` 变为 `data_index` | `GlobalSSBOBufferRegistry.h:57-60`；`RenderPrimitiveCollectSystem.cpp:801,815` |
| `MaterialInstanceAddresses{payload_address, texture_reference_address}` | 字段改名 `{payload_index, texture_reference_index}`（8B），语义为「指向全局池/纹理引用池的行号」而非地址 | `inc/hgl/graph/ShaderBufferSources.h:129-140`；写者 `src/ecs/support/PrimitiveBatchPipeline.cpp:975-994`；读者 `src/ShaderGen/compile/MaterialShaderEmitter.cpp:295,323` |
| `MeshDrawParams` 88B（24B 头 + 8×uint64），按 DrawBatch 写行 | 拆成两级：**几何参数行 `MeshDrawParams` 112B**（24B 头 + **11×uint64**，进全局池按 `geometry_id` 索引，几何创建期写一次）+ **命令参数行 `MeshDrawCommand` 8B**（`geometry_id` + `first_instance`，每批 per-DrawBatch 写，按 `gl_DrawID` 索引） | `inc/hgl/graph/ShaderBufferSources.h:16-88`（MDP 断言 112）/:93-127（MDC 断言 8）；`src/SceneGraph/VKGeometry.cpp:112-176`；`src/ecs/support/PrimitiveBatchPipeline.cpp:591-730` |
| `MaterialBatch::mesh_draw_params_buffer` 是 88B 参数表 | 该成员现在装的是 **`MeshDrawCommand` 表**（容量以「行数」计，`mesh_draw_params_capacity`），表名 `ECS:Batch:MeshDrawCommands` | `inc/hgl/ecs/core/MaterialBatch.h:55-56`；`src/ecs/support/PrimitiveBatchPipeline.cpp:624-640` |
| `RootAddresses` 56B / 7 张表 | **72B**（8×uint64 + `camera_id` + pad）：新增 `addr_texture_references`，`addr_mesh_draw_params` 语义变为「本批 MeshDrawCommand 表」，文本从 2 表扩到 3 表 | `inc/hgl/graph/ShaderBufferSources.h:200-264`；`inc/hgl/graph/RootAddressPush.h:28-64` |
| 相机/天空走 UBO（`camera` UBO 直读） | 相机改为 **SSBO 行池**（`GlobalSSBOType::CameraInfo`，64 行）+ `pc_root.camera_id` 索引；Set0/binding0 与 binding1 仍在绑定表里但设备级一次性挂载由 `RenderSceneUBOSystem::ApplyResourceLayoutBindings` 完成 | `ShaderLibrary/ubo/scene_ubo.glsl:123`；`inc/hgl/graph/ssbo/SSBOTypes.h:20`；`src/ecs/systems/render/RenderSceneUBOSystem.cpp:378-410` |
| 阶段名 `RenderResourceSetup` / `RenderMaterialBind` / `RenderBeginFrame` / `RenderPostProcess` | **枚举中不存在**（4 名全仓 0 命中）。真实相位共 15 项，渲染段为 `RenderSwapchainNextImage → RenderPreBeginFrame → RenderCollect → RenderBatch → RenderBufferCommit → RenderBufferUpload → RenderFrameSync → RenderDrawSubmit → RenderDebug → RenderStat → RenderSubmit` | `inc/hgl/ecs/core/System.h:22-48` |
| 相位用数字（9/10/11/12/13/14/16）指代 | 数字标签已废弃，一律用 `ExecutionPhase` 枚举名。0-based 序：0 TickInput,1 TickTransform,2 TickCamera,3 TickPostCamera,4 RenderSwapchainNextImage,5 RenderPreBeginFrame,6 RenderCollect,7 RenderBatch,8 RenderBufferCommit,9 RenderBufferUpload,10 RenderFrameSync,11 RenderDrawSubmit,12 RenderDebug,13 RenderStat,14 RenderSubmit | `inc/hgl/ecs/core/System.h:24-48` |
| `RenderFrameUBOSyncSystem`（`RenderFrameSync` 相位上） | 该类型 0 命中。`RenderFrameSync` 相位上只有 `RenderSceneUBOSystem`（原 `RenderDescriptorBindingSystem`，2026-09-08 改名） | `src/ecs/systems/render/RenderSceneUBOSystem.cpp:99`；`inc/hgl/ecs/systems/render/RenderSceneUBOSystem.h:35-43` |
| `CreateDefaultLinearGraph`（`use_adaptive_render_graph==false` 的线性图） | 已删除，建图只有 `CreateAdaptiveRenderGraph`；`use_adaptive_render_graph` / `SetAdaptiveRenderGraphEnabled` / `CreateDefaultLinearGraph` 全仓 0 命中。图缓存键是 `SceneStats::GetHash()`，只在 `scene_structure_dirty` 时重算 | `inc/hgl/ecs/core/RenderGraph.h:100-145`；`src/ecs/core/RenderGraph.cpp:217-262`；`src/ecs/core/Context.cpp:598-618` |
| `RenderTo` 尚未成文/离屏 pass 靠 `RenderContext` 副本 | `RenderTo(const RenderPassRequest&)`（`inc/hgl/ecs/core/RenderPassRequest.h`）是离屏/子 pass 一等入口：内部复用 `BeginManagedRenderFrame(…, need_swapchain_acquire=false, &RenderPassOptions)` + `RenderDrawOnly` + `EndManagedRenderFrame`；支持 `camera`（pass 级相机覆盖）、`clear/use_target_clear`、`load_depth`、`use_scissor/scissor`、`clear_scissor_depth`、`mobility_filter`，并同步 `RenderTargetSystem` 的 RT、两侧 `WaitFence` 保护共享 Camera UBO/L2W ring。实际使用者：`example/Basic/ShadowMap.cpp:1097`、`example/Basic/CascadeShadowMap.cpp:683,704`（均为 `RenderTo(const RenderPassRequest&)`）；另外 `src/SceneGraph/module/OffscreenWorld.cpp:149` 用的是旧的三参重载 `RenderTo(graph::IRenderTarget*, Color4f, float)`（`src/ecs/core/Context.cpp:549`），不经 `RenderPassRequest` | `src/ecs/core/Context.cpp:445-557`；`inc/hgl/ecs/core/Context.h:236-249` |
| pass 内 `renderTarget` 字段可切 RT | `RenderGraph::Pass::renderTarget` **尚未生效**：`ExecuteRenderGraphPasses` 不读它，非当前 RT 只打警告，因为 cmd buffer 由帧初始 RT 持有、跨 RT 切换同步未实现 | `inc/hgl/ecs/core/RenderGraph.h:38-48`；`src/ecs/core/RenderGraph.cpp:110-118` |
| `MAX_FRAMES_IN_FLIGHT`(3) 与 retire 延迟「恰好相等、零余量」 | 渲染层已无该常量（全仓 0 命中）。帧同步槽位归 RT 自持，retire 延迟为 `MaterialTextureConfigurationRetireEpochDelay = 3u` | `inc/hgl/graph/module/MaterialTextureReferencePool.h:15` |
| Plan 里未出现的新机制：4-ID 一级表 + 二级索引 + 连号折叠 / IndirectMeshDraw 命令表 / GPU-Driven 覆盖 | 已落地：`RenderItemDescriptor`(16B) + `RenderItemDataStorage` + `DrawItemIDStorage` + `CompactRenderItemHandles` 连号折叠；`MaterialBatch::gpu_driven_override` / `uses_render_item_resolve` / `icb_count_buffer` 支持 100% GPU-Driven 批次（跳过 CPU 侧 ICB 与行表生成） | `inc/hgl/graph/render/RenderItemDescriptor.h:19-44`；`inc/hgl/ecs/support/DrawItemCompaction.h:15-70`；`inc/hgl/ecs/core/MaterialBatch.h:53,69-76`；`src/ecs/support/PrimitiveBatchPipeline.cpp:920,646,1010` |
| 名「恒零行 / 零行占位」 | 现名 **预留行**（`ActiveRowPool::reserve_rows`），池级概念，行 0 不参与分配；纹理引用池与所有全局池一律 `reserve_rows=1` | `inc/hgl/vk/buffer/ActiveRowPool.h:44,96`；`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp:13-20` |

**未能核实（本文未改动、未确认，勿引用本文作为其结论）**：

- 本基线**未实跑** `SimpleSphere` 示例：§2 的每帧系统序是按相位 + 注册先后推定的，§7 只保留代码中的日志格式串与由代码直接推出的算术关系；旧版文档中的实测地址值（如 `0x306380030`）与具体行数统计未复测。
- `SwapchainRenderTarget` 的帧在飞槽位数（`sync_slots` / `slot_count`）未追到赋值点，故 §3.5/§8 的 retire 延迟只断言「3u 是当前值」，不断言与其相等或零余量。
- 示例未跑，未取得当前基线的 `[IndirectMeshDraw] mesh indirect flush engaged: first=… count=…` 实际数值。
