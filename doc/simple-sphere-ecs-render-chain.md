# SimpleSphere 到 vkCmd 的完整渲染链与数据流（技术文档）

> 基线：**2026-09-13 分支 `GPUDriven4ID_1_MaterialDataBuffer`**（`ActiveRowPool/ActiveRowView/ActiveRowLease` 行池统一 + 材质数据全字节化之后）。
> 入口示例：`example/Basic/SimpleSphere.cpp`（私有缓冲几何 + `Lit` PBR 材质 + 单球体 ECS 实体）。
> 目的：以单个最小示例为剖面，记录「作者侧 API → 每帧 CPU 物化 → 一条 vkCmd 提交 → shader BDA 取数」的完整链路与行号证据，作为改渲染数据流时的对照基线。
> 全部 path:line 在本文基线核实；本文只描述现状，不含改造建议。

---

## 1. 总览：数据分三层落地

| 层 | 时机 | 干什么 | 产物 |
|---|---|---|---|
| ① 作者侧 | `Init()` 一次 | 申请材质行、建几何、定材质定义、装纹理 | 行号 `data_index`、VAB/IBO、shader program、bindless 句柄 |
| ② 每帧 CPU 物化 | phase 9–13（每帧） | 行号→GPU 地址、建两张行表、建 mesh 命令、上传脏段、挂 UBO | `MeshDrawParams` 行、`MaterialInstanceAddresses` 行、间接命令 |
| ③ GPU 侧 | 一条 `vkCmdDrawMeshTasksIndirectEXT` | mesh shader 靠 `pc_root` + `gl_DrawID` + `gl_InstanceIndex` 直读全部数据 | 三角形 |

关键事实：**没有任何 per-draw 描述符、没有 `vkCmdBindVertexBuffers`、没有 vertex input state**。索引/顶点/变换/材质/纹理全部走 buffer_reference（BDA）解引用；描述符集只剩 2 个（Scene UBO / Bindless 纹理），且首次绑一次全局复用。

---

## 2. 入口 → 帧循环

```
os_main                                     example/Basic/SimpleSphere.cpp:261
└ RunFramework<SimpleSphereApp>(...)         inc/hgl/framework/WorkManager.h:77
  └ 建 GraphicsContext / ECSContext / 各 Manager → SimpleSphereApp::Init()
  └ WorkManager::Run 帧循环                   src/Work/WorkManager.cpp:86-126
    ├ Tick(wo)  → WorkObject::Tick → 示例 Tick（球体自转）        :14-32
    └ Render(wo) → wo->GetECSContext()->Render(dt, pre_render)     :58-69
      └ ECSContext::Render                    src/ecs/core/Context.cpp:530
        └ RenderGraph “1 pass”（CreateDefaultLinearGraph：把每个已注册系统组
          变成一段启用 pass：startPhase..endPhase + runUpdate + runRender）
                                             src/ecs/core/RenderGraph.cpp:292-333
```

单帧骨架（`BeginManagedRenderFrame`，`src/ecs/core/Context.cpp:395-427`）：

```
AcquireSwapchainImage
→ RenderPreBeginFrame（ResourceSetup / MaterialBind）
→ BeginFrame（打开 cmd buffer）
→ PrepareRenderPassSetup()                    Context.cpp:684-698
    SetFrameIndex → RenderBeginFrame → RenderCollect(9) → RenderBatch(10)
    → RenderBufferCommit(11) → RenderBufferUpload(12) → RenderFrameSync(13)
→ BeginRenderPass()  src/ecs/systems/render/RenderSystemCore.cpp:81-95
    → RenderCmdBuffer::BeginRendering（Dynamic Rendering）
      src/Vulkan/VKCommandBufferRender.cpp:157 vkCmdBeginRendering
      附件 = swapchain 图，clear 值必须在此之前写入
→ pass 0 的各 render phase（14/16 在此真正画）
→ EndFrame() → EndRenderingPresent（color → PRESENT_SRC 布局转换）
→ SubmitFrameToRenderTarget（fence 提交）
```

实测一帧的系统序（本示例运行日志，括号内为 `ExecutionPhase`）：

```
(9)  RenderPrimitiveCollectSystem → PrimitiveCullSystem → PrimitiveSortSystem
(10) PrimitiveBuildSystem
(11) ViewUBOCommitSystem
(12) RenderBufferUploadSystem
(13) RenderFrameUBOSyncSystem → RenderSceneUBOSystem
(14) PrimitiveRenderSystem          ← 唯一发出 vkCmdDraw* 的系统
(16) PrimitiveOverlayRenderSystem
```

---

## 3. Init 阶段：数据在 CPU 侧怎么落地

### 3.1 材质参数行 → 行租约（行号就是身份）

- 申请：`GetManager<MaterialSSBOBufferRegistry>()->GetMaterialDataAccessor<graph::ssbo::PBRSurfaceRow>()`（SimpleSphere.cpp:94）
- 池在创建期按 `MaterialSSBOType` 枚举遍历建好：`ActiveRowPool material_row_pools[RANGE_SIZE]`，容量 `DefaultMaterialDataElementCapacity = 1024`，超限 fail-fast（`inc/hgl/graph/module/MaterialSSBOBufferRegistry.h:83-88`）
- 池本体 = `device->CreateArenaBuffer(name, row_bytes*capacity)` + `GetBufferDeviceAddress[(Aligned16)]` + 整块 `Map` + `memset 0` + `MarkDirty`（`src/Vulkan/buffer/ActiveRowPool.cpp:34-76`）
- 写入：`acc.Write(row)` → `ActiveRowView::WriteAs` → `WriteAt(0,…)`（`memcpy` 后 `CommitByID`）→ `ActiveRowPool::CommitRow` = 按行 `MarkDirty(id*row_bytes, row_bytes)`（`inc/hgl/vk/buffer/ActiveRowView.h:174-187`、`src/Vulkan/buffer/ActiveRowPool.cpp:191-203`）
- 返回 `GetMaterialSSBOBinding() = {ssbo_type, ssbo_id, row_id}`，同时喂两处：recipe 的 `material_ssbo_binding` 与组件的 authoring 资源（SimpleSphere.cpp:118 / :190）。**`row_id` 即 shader 侧 `data_index`**。

### 3.2 recipe 与资产

`MaterialRecipe{ recipe_name, mtl_def_id="Lit", pipeline_config=MakeSolid3DConfig(), material_ssbo_binding }` → `PrimitiveAsset(geometry, &recipe, PrimitiveType::Triangles)` → `PrimitiveComponent::SetPrimitiveAsset / SetMaterialDataResource / SetMaterialTextureResource`（SimpleSphere.cpp:165-193）。

### 3.3 几何（私有路径）

- `GeometryCreater geometry_creater(device, CreateStandardGeometryVertexFormat())`（3 流：Position V3F / TexCoord V2HF / Normal V2UN8）→ `CreateSphere(pc, 64)` → VAB（带 `SHADER_DEVICE_ADDRESS` usage）+ U32 索引；几何对象登记进 `GeometryManager`（SimpleSphere.cpp:140-163）
- 运行时包装在首次解析时生成：`runtime_data_buffer = new GeometryDataBuffer(gvf.GetCount(), geometry->GetIBO(), geometry->GetVDM())` 后 `Update(geometry)` 按语义把每个 VAB 的 `VkBuffer` 填进 `vab_list/vab_semantic`（`src/ecs/components/PrimitiveComponent.cpp:139-174`、`src/SceneGraph/mesh/Primitive.cpp:114-140`）
- 私有路径与 VDM 路径的差别只在 `geometry->GetVDM()!=nullptr`（共享池段 vs 每几何独立缓冲）；`GeometryDrawRange` 的 `vertex_offset/first_index` 恒 0（私有）或段偏移（VDM）。

### 3.4 材质定义 → program → pipeline

- `ShaderLibrary/material/lit.material.toml`：`[vertex] requirements=["Position","UV0","Normal"]`，`[resources] textures = 6 个可选槽 + ubos=[CameraInfo,SkyInfo]`
- ShaderGen 编译成 `program-<hash>`；`RenderPrimitiveCollectSystem` 取到 program 后 `render_pass->CreatePipeline(program, recipe)`（`src/ECS/systems/render/RenderPrimitiveCollectSystem.cpp:640` → `src/Vulkan/VKRenderPass.cpp:103-160`），key = shader stages + 附件格式 → 复用到 1 条 mesh pipeline（日志 `[RenderPass::CreatePipeline] Created Pipeline 'program-…'`）

### 3.5 纹理 → bindless 句柄 + 引用池

- `LoadTexture2D` 得到 `Texture2D*`；首次 collect 时 `rdbs->RegisterTextureResource(...)` → `BindlessTextureManager::RegisterTexture` 返回 **1-based 句柄**（`src/ecs/systems/render/RenderSceneUBOSystem.cpp:220-234`、`src/Vulkan/VKBindlessTextureManager.cpp:127`）
- 按材质定义建引用池（每 definition 一池）：`[MaterialTextureReferencePool] created definition=Lit references=6 row_stride=48 capacity=1024 bytes=49200`
  - 行宽 = 6×`uvec2` = 48B；`bytes = (1024+1)×48`，**行 0 为恒零行占位**（`material_texture_zero_row_gpu` 来源）
  - 本实体落在 **行 1** → `material_texture_row_gpu = 0x306380030`（= 池基址 + 1×48 ✓）

---

## 4. 每帧 CPU 物化（phase 9–13）

1. **phase 9 `RenderPrimitiveCollectSystem`**（`src/ECS/systems/render/RenderPrimitiveCollectSystem.cpp`）
   - 解析 program / 构造 effective recipe / 校验（缓存 hash 命中即跳过）
   - **材质行地址物化**：`material_row_gpu = material_buffer.gpu_base + data_index*row_bytes`（:797-802）；前置校验 `TryGetRowBuffer`（段查询）+ `IsMaterialDataIDActive`（行号活跃）+ `data_index < row_capacity`（:750-795）；同时填 `material_row_cpu`
   - **纹理引用行**：算配置 hash → 复用或 `AcquireMaterialTextureConfiguration` + `WriteMaterialTextureConfiguration`（:960-1018）→ `material_texture_row_gpu` / `material_texture_zero_row_gpu`（:1025-1041）；行内容 = 每个纹理声明一个 `uvec2{handle, array_layer}`（:860-945）
2. **phase 9 Cull / Sort** → **phase 10 `PrimitiveBuildSystem` → `PrimitiveBatchPipeline`**（`src/ecs/support/PrimitiveBatchPipeline.cpp`）
   - `EnsureMeshDrawParams` 建参数表 buffer 并整表清零（:452-502）
   - `BuildBatches` 生成 DrawBatch：同 `GeometryDataBuffer` + 同 `GeometryDrawRange` 的相邻项合并为实例（:354-450）
   - `WriteMeshDrawCommands`：每 DrawBatch 写一行 **`MeshDrawParams`（88B = 24B 头部 + 8×uint64）**：`index_base / vertex_base / is_indexed / total_vertices / first_instance` + 8 个 `addr_*`（各流 VAB 与 IBO 设备地址，`dev->GetBufferDeviceAddressAligned16`，:504-601）；同时写 mesh 命令 `{groupCountX=ceil(顶点数/组), groupCountY=instance_count, Z=1}`（:607-636）
   - `WriteBatchIndexRows`：写 **每实例**两份行——`l2w_index`（transform 槽位）与 `MaterialInstanceAddresses{payload_address=material_row_gpu, texture_reference_address}`（:786-888）
3. **phase 11 `ViewUBOCommitSystem`**：`CommitCameraUBO` / `CommitViewportUBO` / 环境材质化提交（`src/ecs/systems/render/ViewUBOCommitSystem.cpp:22-42`）
4. **phase 12 `RenderBufferUploadSystem`**：脏段 staged → GPU 上传
5. **phase 13 `RenderSceneUBOSystem`**：`ApplyResourceLayoutBindings` 把 camera/sky/viewport UBO 挂进全局 Scene set（`src/ecs/systems/render/RenderSceneUBOSystem.cpp:324-336`——Scene 集的数据写入者，勿删）

---

## 5. 提交绘制（phase 14 → vkCmd）

`PrimitiveRenderSystem`（`src/ecs/support/primitive/PrimitiveRenderSystem.cpp:39-93`）按 `MaterialBatch::key` 排序后逐批调用渲染器；`PipelineMaterialRenderer::Render`（`src/ecs/support/PipelineMaterialRenderer.cpp:90-187`）对每个批次只做四件事：

| 步 | 代码 | 内容 |
|---|---|---|
| 1 | :110 | `BindPipeline(pipeline)` —— pipeline 只含 shader 部分 |
| 2 | :114 | `ApplyPipelineState(pipeline->GetConfig())` —— EDS 1/2/3 全套 `vkCmdSet*`（cull/depth/blend/polygon/…） |
| 3 | :121 | `BindGlobalDescriptorSets(cmd, layout)` —— Scene(0)/Bindless(1) 每 cmd 首绑一次（`scene_sets_bound` 守卫） |
| 4 | :155-166 | `PushRootAddresses(…, 56B)` —— `pc_root`：7 张全局表设备地址（本路径真用 mesh_draw_params / l2w / l2w_index / mtl_data_addrs） |
| 5 | :37-61、:183-186 | 累积 DrawBatch 计数 → 批末 `cmd_buf->DrawMeshTasksIndirect(icb_mesh_tasks, first*sizeof(cmd), count)` |

最终落点：

```
src/Vulkan/VKCommandBufferRender.cpp:269-276
  dev_attr->cmd_draw_mesh_tasks_indirect(cmd_buf, buffer, offset, drawCount, stride)
  → pfn vkCmdDrawMeshTasksIndirectEXT（设备创建时 vkGetDeviceProcAddr 加载）
```

一次 multi-draw 提交本批**全部** DrawBatch：命令序 = DrawBatch 序 = 参数行序，每条命令靠 `gl_DrawID` 找自己的行。

---

## 6. GPU 侧：shader 怎么把数取回来

| 需要什么 | 从哪取（生成文本/源文件） |
|---|---|
| per-draw 参数 | `draw_params = MeshDrawParamsRef(pc_root.addr_mesh_draw_params).rows[gl_DrawID]`（`src/ShaderGen/meshgen/MeshTemplateEmitter.h:258`；结构体字段由 `inc/hgl/graph/ShaderBufferSources.h:16-30` X 列表单一真源发射） |
| 顶点号 | 非索引：`WorkGroupID.x*组大小 + LocalInvocationIndex`；索引：`sbo_vertex_index.data[index_base + 序号]`（`src/ShaderGen/meshgen/MeshShaderModeVertexPassthrough.h:58-62`） |
| Position | `VertexPositionRef(draw_params.addr_position).data[vertex_base + VertexIndexID]`（`ShaderLibrary/vertex/s1_position_vec3.glsl:27`） |
| UV | `unpackHalf2x16(sbo_vertex_uv.data[vertex_base + idx])`（`ShaderLibrary/vertex/s1_uv_rg16f.glsl`） |
| Normal | RG8 packed uint → octahedral 解码（`ShaderLibrary/vertex/s1_ntb_rg8.glsl:23`） |
| 世界变换 | `GetL2W()` = `LocalToWorldDataRef(pc_root.addr_l2w).mats[…]`（`ShaderLibrary/common/l2w_ssbo.glsl`），下标经 `l2w_index` 行表 |
| 每实例材质行 | mesh 阶段 `fragDataIndexID[vid/3] = gl_InstanceIndex`（`MeshShaderModeVertexPassthrough.h:75`）→ FS 宏 `MTL_ROW(i) = PBRSurfaceRowRef(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].payload_address)`（发射于 `src/ShaderGen/compile/MaterialShaderEmitter.cpp:190`） |
| 纹理 | `MTL_TEX(i) … .texture_reference_address` 解出 `uvec2(handle, layer)`（`MaterialShaderEmitter.cpp:217`）→ `bindless_tex[nonuniformEXT(handle-1)]` + `bindless_samp[采样器常量]`（`ShaderLibrary/common/bindless_textures.glsl:33-53`）；消费点 `ShaderLibrary/material/pbr_surface_source.glsl:26/39` |
| 相机/天空 | Scene 集（set=0）的 CameraInfo / SkyInfo UBO |

`draw_params` 相关约定（`inc/hgl/graph/ShaderBufferSources.h:59-85`）：std430 全标量无 padding，头部 6×4B + 8×8B 地址 = 88B，`static_assert` 编译期锁死；`RootAddresses` = 7×uint64 = 56B（:141-158）。

---

## 7. 实跑日志自洽性校验

```
[DeferredResource] owner=SphereEntity program=program-1126516709271066536 planned_texture=3 planned_data=1
[MaterialTextureReferencePool] created definition=Lit references=6 row_stride=48 capacity=1024 bytes=49200
[MaterialTextureReferences] owner=SphereEntity definition=Lit row=1 references=6 gpu=0x306380030
[ArenaDebug] rows written: n=1 payload0=0x302280000 texture0=0x306380030
[IndirectMeshDraw] mesh indirect flush engaged: first=0 count=1
```

- `0x306380030 − 0x306380000 = 48 = row_stride` → 印证“行地址 = 池基址 + 行号 × 行距”；
- `payload0 = 0x302280000` = PBRSurface 行池基址 + `data_index(0) × 32B`；
- 两地址分别为 `MaterialInstanceAddresses` 的两列，与 `PrimitiveBatchPipeline.cpp:838-861` 写入、`MaterialShaderEmitter.cpp:190/217` 读取一一对应；
- `(1024+1)×48 = 49200` 印证纹理引用池多出的 1 行即“恒零行”。

复现：`cmake --build build --config Debug --target SimpleSphere` → 设 `ULRE_ARENA_DEBUG=1` 起 `build/out/Windows_64_Debug/SimpleSphere.exe`（cwd = 仓库根，`res/` 在根）。

---

## 8. 改动这套链时的雷区（现状约束）

1. **身份 = 物理行号**：`data_index` 是池内行号，不是“块内偏移”；材质行池 1024 行不扩容，超限 fail-fast（按用户口径 = 项目 bug，不是设计缺陷）。
2. **寻址只靠两个内建索引**：per-draw 用 `gl_DrawID`（参数表），per-instance 用 `gl_InstanceIndex`（材质/纹理/变换行表）。因此“命令序 = DrawBatch 序 = 行序”是硬约束，任何新增/合并 DrawBatch 的改动都要同步维护三处行序。
3. **56B `pc_root` 每批 push 一次**，push 的是**表地址**而非 per-draw 数据——一次 multi-draw 内 N 条命令共享一份，不违反合批。任一渲染路径漏 push → 解引用 0 → 静默黑屏（无 VUID、无 GPU fault）。
4. **纹理引用池的 retire 延迟（3 帧）与渲染层 `MAX_FRAMES_IN_FLIGHT`(3) 恰好相等、零余量**：改帧在飞数或加深提交管线必须同步加大该延迟，否则回收清零会落在仍被 GPU 读的行上（症状：静默丢纹理）。
5. **三个渲染路径（Primitive / Line / Text）都必须 `BindPipeline` 后紧跟 `ApplyPipelineState`**，且各自 push 自己消费的表（Text 不消费 l2w）。
