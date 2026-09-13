# ULRE ECS 层架构与帧流程详解（技术文档）

> 基线：**2026-09-13 分支 `GPUDriven4ID_1_MaterialDataBuffer`**，trace 载体 = `example/Basic/SimpleSphere.cpp`（单球体 + `Lit` 材质，一个 Primitive 实体 + 一个 Camera 实体）。
> 范围：`inc/hgl/ecs/` + `src/ecs/` 全部分层（core / components / systems(tick,render) / support），以及 ECS 与渲染层（Graph/Vulkan）的接口面。
> 与 `doc/simple-sphere-ecs-render-chain.md` 的分工：那篇讲「数据怎么从作者 API 走到 vkCmd」；本篇讲「ECS 层内部由谁、在什么阶段、按什么顺序驱动这些数据」。
> 全部 path:line 在本基线核实；本文只描述现状。

---

## 1. 分层与两个循环

```
inc/hgl/ecs/
├ core/        Object / Component / Entity / EntityHandle / EntityManager / Context
│              System / SystemGroup / SystemProfiler / RenderGraph
│              RenderItem / PrimitiveRenderItem / MaterialBatch / ShaderProgramPipelineKey
├ components/  Transform / Primitive / Material / Camera / Visibility / BoundingBox / Lines / Text / Renderable
├ systems/
│  ├ tick/     InputSystem / TransformSystem / CameraSystem / VisibilitySystem
│  │           BoundingBoxUpdateSystem / LineBoundsUpdateSystem
│  └ render/   RenderPrimitiveCollectSystem / RenderBufferUploadSystem / RenderFrameUBOSyncSystem
│              ViewUBOCommitSystem / RenderSceneUBOSystem / RenderSystemCore
│              RenderTargetSystem / SwapchainNextImageSystem / SwapchainSubmitSystem
│              EnvironmentSystem / ColorPaletteSystem / LineStatsSystem
└ support/     RenderPipelineBase / RenderPipelineSystem / PrimitiveBatchPipeline / MaterialBatch(渲染器)
               PipelineMaterialRenderer / PrimitiveRenderPipeline / LineRenderPipeline / TextRenderPipeline
               TransformAssignmentBuffer / TransformDataStorage / VisibilityDataStorage
               BoundingBoxDataStorage / RenderResource / PositionSourceSpec / TransformPolicySpec
```

两个互相独立的循环（`src/Work/WorkManager.cpp:78-126`）：

| 循环 | 入口 | 遍历对象 | 内容 |
|---|---|---|---|
| **Tick 循环** | `ECSContext::Tick` `src/ecs/core/Context.cpp:336-366` | `tick_system_order` + 所有 Entity | tick 系统 `Update()` → 每个 Entity `OnUpdate()` → InputSystem::EndFrame |
| **Render 循环** | `ECSContext::Render` `src/ecs/core/Context.cpp:530-570` | `render_system_order`（按 phase 分段） | RenderGraph 各 pass 的 Update/Render 两遍扫描 + 帧首尾的 Begin/End/Submit |

两者**共用一套系统注册表与拓扑排序**，靠 `ExecutionPhase` 的数值分界自动分派（见 §3.2）。

---

## 2. 核心对象模型

### 2.1 对象三段式

| 类 | 基类 | 身份 | 生命周期动词 | 关键成员 |
|---|---|---|---|---|
| `Object` | — | `ObjectID objectId`（自增） | `OnCreate/OnUpdate/OnDestroy` | 名称 `objectName` `inc/hgl/ecs/core/Object.h:15-68` |
| `Component` | `Object`? → 实为 `enable_shared_from_this` | 归属 `EntityID owner_id` + `ECSContext* owner_context` | `OnAttach/OnDetach/OnUpdate` | `version` / `change_mask`；`GetSystemGroupName()`（默认 nullptr）`inc/hgl/ecs/core/Component.h:19-47` |
| `Entity` | `Object` | `EntityID id`（由 EntityManager 分配） | `OnCreate/OnUpdate`（转发组件） | `UnorderedMap<hash_code, shared_ptr<Component>>` `inc/hgl/ecs/core/Entity.h:22-32` |
| `System` | `Object` | `typeid(T).hash_code()` 作为注册键 | `OnDependenciesReady / Initialize / Update / Render / Shutdown` | phase / dependencies / enabled / element_type `inc/hgl/ecs/core/System.h:62-127` |

生命周期动词是**三套不同语义**，不要统一（`inc/hgl/ecs/core/Object.h:54-58` 注释明示）：Object 创建销毁 / Component 挂载卸载 / System 初始化运行。

### 2.2 组件挂载 → 自动反向注册

`AddComponent<T>()` → `Entity::ReplaceComponent`（`src/ecs/core/Entity.cpp:37-54`）：

```
components[hash] = component
component->SetOwner(entity_id, context)     // 组件持回链，可 GetOwner()
RegisterToContext → ECSContext::RegisterComponentInstance   ← 关键：向 Context 反向登记
component->OnAttach()
MarkSceneDirty()  → context->MarkSceneStructureDirty()      ← 触发 adaptive 图重算
```

`RegisterComponentInstance`（`src/ecs/core/Context.cpp:1198-1229`）做三件事：

1. 按类型 hash 存进 `component_registry`（weak_ptr 列表）——`GetComponents<T>` 就是遍历这张表（`inc/hgl/ecs/core/Context.h:686-706`）；
2. 若组件声明了 `GetSystemGroupName()`（如 `PrimitiveComponent` 返回 `"Primitive"`，`inc/hgl/ecs/components/PrimitiveComponent.h:137`）：`system_group_component_counts[group]++` → **自动安装该组的系统**（`EnsureSystemGroupSystems`）+ **打开该组系统开关**（`SetElementTypeSystemsEnabled(group, true)`）+ 记录 profiler 组状态；
3. 对 `component_query_bases`（`Initialize` 时注册的基类：`RenderableComponent/PrimitiveComponent/MaterialComponent`，`src/ecs/core/Context.cpp:78-80`）再做一次登记，于是 `GetComponents<RenderableComponent>()` 能召回派生子类实例。

反注册对称处理：计数归零即关组（`src/ecs/core/Context.cpp:1231-1266`），另有两个显式入口 `DisableUnusedSystemGroups` / `CleanupSystemGroup`（`src/ecs/core/Context.cpp:1470-1541`）。

### 2.3 Entity 管理

`EntityManager`（`inc/hgl/ecs/core/EntityManager.h`、`src/ecs/core/EntityManager.cpp`）持全部 Entity（`Context` 构造时预分配 1000，`src/ecs/core/Context.cpp:53-58`），EntityID = 句柄（`EntityHandle.h`）。创建走 Context 模板：

```
CreateEntity<Entity>("SphereEntity")                        inc/hgl/ecs/core/Context.h:414-431
  → make_unique<T> → entity_manager->CreateEntity → SetContext(this) → OnCreate()
CreateChildEntity(parent, desc)                             src/ecs/core/Context.cpp:152-178
  → 建 Entity + TransformComponent + SetLocalTRS + SetParent + 回填 id
```

`TransformComponent` 有专门的旁路索引：`Context::RegisterTransformComponent / MigrateTransformComponent / UnregisterTransformComponent` 维护 `static_transforms` / `movable_transforms` 两个 weak_ptr 向量（`inc/hgl/ecs/core/Context.h:134-135`、`src/ecs/core/Context.cpp:1292-1398`）——Tick 系统靠它们做到「只遍历 movable」。

---

## 3. 系统模型与阶段模型

### 3.1 ExecutionPhase：全引擎唯一的时间轴

`inc/hgl/ecs/core/System.h:22-56`（枚举值即顺序）：

| 区间 | 阶段 | 语义 |
|---|---|---|
| Tick | `TickInput` | 输入 |
| Tick | `TickTransform` | 变换 / 包围盒 / 可见性 |
| Tick | `TickCamera` | 相机矩阵 |
| Tick | `TickPostCamera` | 面向策略变换 |
| Render 前（cmd 未开） | `RenderSwapchainNextImage` | 取 swapchain 图 |
| | `RenderPreBeginFrame` | 每帧环境 / viewport 同步 |
| | `RenderResourceSetup` | 惰性一次性 GPU 资源 |
| | `RenderMaterialBind` | 每实体材质/纹理绑定 |
| 前段 CPU（cmd 已开、pass 外） | `RenderBeginFrame` | 开 cmd、录帧 UBO |
| | `RenderCollect` | 收集/剔除可见组件 |
| | `RenderBatch` | 建批、写 VAB（staged 写） |
| | `RenderBufferCommit` | 收尾 CPU 写 |
| | `RenderBufferUpload` | GPU 传输 vkCmdCopyBuffer |
| | `RenderFrameSync` | 上传后同步 UBO/描述符 |
| pass 内 | `RenderDrawSubmit` | 录绘制命令 |
| | `RenderPostProcess` / `RenderDebug` | 后处理 / 调试叠加 |
| pass 后 | `RenderStat` / `RenderSubmit` | 统计 / 提交呈现 |

分界值 = `RenderSwapchainNextImage`：`AddOrUpdateSystem` 用它判定「tick 系统 or render 系统」，注册方传错会自动纠正并告警（`src/ecs/core/Context.cpp:969-979`）。

### 3.2 系统注册

```
RegisterTickSystem<T>(args...)     inc/hgl/ecs/core/Context.h:511-515  → RegisterTickSystemScoped(Auto)
RegisterRenderSystem<T>(args...)   inc/hgl/ecs/core/Context.h:517-522  → RegisterRenderSystemScoped(Auto)
GetSystem<T>()                     （type hash 查 tick_systems / render_systems）
```

Scoped 版本带两道门（`inc/hgl/ecs/core/Context.h:524-595`）：`SystemOwnershipScope{GlobalShared, LocalIsolated, Auto}` × `ContextRole{RootShared, LocalSubWorld}`——子世界不能注册全局共享系统，反之亦然；被拒会累加 `rejected_render_system_registration_count`。简单示例（`RootShared`）永远走 Auto，不受影响。

`AddOrUpdateSystem`（`src/ecs/core/Context.cpp:961-1063`）落地：

- 按 phase 决定进 `tick_systems` 还是 `render_systems`；
- 非空 `render_element_type` 时把系统登记进 `systems_by_element_type[type]`（`src/ecs/core/Context.cpp:1014-1030`）——这是「整组开关」的抓手；
- 维护 `OrderedSystem{key, phase, insertion_order, system}` 列表，置 `*_order_dirty`；
- 自动把 `system->GetDependencies()` 声明的依赖写进依赖图；
- 若 Context 已 active，立即 `OnDependenciesReady()` + `Initialize()`（运行期热插拔系统）。

### 3.3 拓扑排序 = 相位优先 + 插入序稳定 + 环回退

`SortSystemList`（`src/ecs/core/Context.cpp:840-948`）用 Kahn 拓扑排序，**可用集比较器是 `(phase, insertion_order)`**（:891-900）：

- 依赖只表达「必须先于」，相位表达「阶段」，两者同时满足时按注册先后；
- 出现环 → 打印告警并整体回退 `stable_sort(phase, insertion_order)`（:929-941）；
- 仅在 dirty 时才重排（:845-846），排序结果缓存在 `tick_system_order` / `render_system_order`。

### 3.4 初始化顺序

`Context::Initialize`（`src/ecs/core/Context.cpp:65-150`）：

```
1 绑定 device / render_target；把 device 传播给已注册的 RenderBufferUploadSystem
2 注册组件查询基类（Renderable/Primitive/Material）
3 确保基础系统存在：TransformSystem(tick) / VisibilitySystem(tick) / RenderBufferUploadSystem(render)
4 SortTickSystems + SortRenderSystems
5 按顺序对每个系统执行 OnDependenciesReady() → Initialize()      ← 严格按拓扑序
6 active = true; OnCreate()
```

`DefaultSystems.cpp` 再补上「核心 + 三个元素组」：

```
EnsureCoreEcsSystems           src/ecs/core/DefaultSystems.cpp:166-210
  InputSystem / CameraSystem / EnvironmentSystem / ColorPaletteSystem
  RenderTargetSystem / SwapchainNextImageSystem / SwapchainSubmitSystem
  RenderFrameUBOSyncSystem / RenderSceneUBOSystem / ViewUBOCommitSystem
  （SetRenderContext / SetViewportInfo / SetRenderTarget 在这里接线）
EnsureSystemGroupSystems(ctx,"Primitive"|"Text"|"Line")   :212-230 → 组安装器
RegisterDefaultEcsSystems      :232-254 → 返回常用系统句柄给 WorkObject
```

组安装器（`src/ecs/core/DefaultSystems.cpp:65-161`）按名字注册到 `SystemGroupRegistry`，内容是「建 pipeline + 注册该组的薄代理系统」：

| 组 | 安装内容 |
|---|---|
| Primitive | `PrimitiveRenderPipeline` + `RenderPrimitiveCollectSystem`（共享） + `RenderBufferUploadSystem`（共享） + Cull/Sort/Build/Render/OverlayRender |
| Text | `TextRenderPipeline` + Collect/Build/Sync/Render |
| Line | `LineRenderPipeline` + `LineCollectSystem/LineBuildSystem/LineRenderSystem` + tick 的 `LineBoundsUpdateSystem` + `LineStatsSystem` |

### 3.5 SystemGroup / 元素类型 / 组开关

- `SystemGroup{name, startPhase, endPhase, enabled}`（`inc/hgl/ecs/core/SystemGroup.h:28-46`）是「一组系统共享的相位区间」；
- 注册表是**静态单例**，支持 `RegisterGroupInstaller` 插件式安装（:52-108），并提供 `ScopedSystemGroupState` 做临时开关（:114-137），旧别名 `RenderSystemGroup/RenderSystemGroupRegistry/ScopedGroupState` 仍在（:139-141）；
- 组元数据在首帧由 `EnsureSystemGroupsRegistered` 从系统集合反推：遍历每个 element type 的系统，取 `min(startPhase)/max(phase)` 作为组区间（`src/ecs/core/RenderGraph.cpp:21-87`，幂等，只补缺失）；
- 「组是否启用」真正生效在两处：图形章节的 pass 构造（§3.6）+ `SetElementTypeSystemsEnabled` 直接 `SetEnabled(false)` 掉该组所有系统（`src/ecs/core/Context.cpp:1421-1435`）。

对 SimpleSphere：只有 `PrimitiveComponent` 存在 → 只有 "Primitive" 组有组件计数 → Text/Line 组的系统在首帧就被关掉；这一点在日志里表现为 pass 只有 1 个、且看不到 Line/Text 系统。

### 3.6 RenderGraph：pass 序列

`RenderGraph::Pass{startPhase, endPhase, renderTarget, enabled, runUpdate, submitTransforms, runRender, onBeforePass, onAfterPass}`（`inc/hgl/ecs/core/RenderGraph.h:30-76`）。两条构造路径：

| 路径 | 内容 | 触发 |
|---|---|---|
| `CreateDefaultLinearGraph`（`src/ecs/core/RenderGraph.cpp:292-333`） | **把每个已注册组变成一段 pass**（`startPhase..endPhase` + update + render），全开 | `use_adaptive_render_graph == false`（示例默认） |
| `CreateAdaptiveRenderGraph`（`inc/hgl/ecs/core/RenderGraph.h:153-160`，`src/ecs/core/RenderGraph.cpp:230-290`） | 由 `SceneStats.active_render_groups` 决定哪些组进图 | `SetAdaptiveRenderGraphEnabled(true)`，场景结构 hash 变化时才重算（`src/ecs/core/Context.cpp:536-558`） |

`EnsureSystemGroupsRegistered` 必须在建图前跑过一次（`src/ecs/core/DefaultSystems.cpp:244`），否则组元数据缺失 → 图里没有对应 pass。

---

## 4. 帧驱动：三遍扫描 + 一次 pass 循环

### 4.1 帧入口

```
ECSContext::Render(dt, pre_render)                        src/ecs/core/Context.cpp:536-570
 └ (adaptive 分支：必要时重算图)
 └ ECSContext::Render(dt, graph, pre_render)              src/ecs/core/RenderGraph.cpp:174-188
    ├ LogInfo("===== Frame Start (RenderGraph with N passes) =====")
    ├ BeginManagedRenderFrame(0.0f)                       src/ecs/core/Context.cpp:383-427
    │   ① AcquireSwapchainImage()       → SwapchainNextImageSystem.NextFrame()
    │   ② RenderPreBeginFrame()         → phase RenderPreBeginFrame + RenderResourceSetup + RenderMaterialBind
    │   ③ SyncRenderTargetViewport()
    │   ④ render_core->BeginFrame()     → 开 cmd buffer
    │   ⑤ SetCurrentRenderCmd(cmd)      ← 后续所有 Render() 都拿它
    │   ⑥ PrepareRenderPassSetup(...)   ← 见 4.2
    │   ⑦ render_core->BeginRenderPass()→ vkCmdBeginRendering（pass 外阶段全部在它之前）
    ├ ExecuteRenderGraphPasses(graph)                     src/ecs/core/RenderGraph.cpp:107-178
    └ EndManagedRenderFrame(0.0f)                         src/ecs/core/Context.cpp:429-449
        EndFrame（EndRenderingPresent）→ SubmitFrameToRenderTarget → 可选 WaitIdle
```

### 4.2 pass 外的 CPU 阶段（`PrepareRenderPassSetup`）

`src/ecs/core/Context.cpp:684-698` 明确「所有 CPU 与上传工作在 BeginRenderPass 之前，pass 内只发 GPU 绘制命令」：

```
SetFrameIndex(frameIndex)                                    ← 帧序号，给 ring/帧缓存用
RenderBeginFrame(dt)          → phase RenderBeginFrame
RunRenderPhaseUpdates(RenderCollect)   → 收集/剔除（RenderPrimitiveCollectSystem、Cull、Sort、Text/Line Collect）
RunRenderPhaseUpdates(RenderBatch)     → 建批/写行表（PrimitiveBuildSystem、Text/Line Build）
RenderBufferCommit(dt)        → phase RenderBufferCommit（ViewUBOCommitSystem：相机/视口/天空 UBO）
RenderBufferUpload(dt)        → phase RenderBufferUpload（遍历脏 GPU buffer 做 CopyToDevice + barrier）
RenderFrameSync(dt)           → phase RenderFrameSync（RenderFrameUBOSyncSystem、RenderSceneUBOSystem）
```

### 4.3 pass 内：一遍 RenderGraph 循环

`ExecuteRenderGraphPasses`（`src/ecs/core/RenderGraph.cpp:114-178`）对每个 pass：

```
onBeforePass（可选）
runUpdate   → RunRenderPhaseUpdates(max(start, RenderDrawSubmit), endPhase)   ← 只跑 pass 内相位
runRender   → RecordPreparedRenderPhaseRange(start, end, dt, submitTransforms) ← 注意用的是完整区间
submitTransforms → TransformSystem::SubmitTransformUpdates()
onAfterPass（可选）
```

`RecordPreparedRenderPhaseRange`（`src/ecs/core/Context.cpp:451-472`）先提交变换，再 `RunRenderSystemsInRange`。

### 4.4 两个遍历函数的区别（容易踩）

| 函数 | 调用哪个虚函数 | 用途 |
|---|---|---|
| `RunRenderPhaseUpdates(phase)` / `(min,max)` `src/ecs/core/Context.cpp:732-765` | `system->Update()` | 渲染阶段的 **CPU 工作**（收集、建批、上传、同步） |
| `RunRenderSystemsInRange(min,max)` `src/ecs/core/Context.cpp:786-818` | `system->Render(cmd, dt)` | pass 内 **录命令**；进出时切 `device->SetDrawPhaseActive(true/false)` |

两者都先 `SortRenderSystems()`；都检查 `IsEnabled()`；都用 `HGL_CAPTURE_SCOPE()` + `SystemProfiler` 计时 + `[ECS] Update/Render Begin|End` 日志。`RunSystemUpdate`（`src/ecs/core/Context.cpp:767-784`）是共用的启用判断+计时包装。

---

## 5. 组件层详解

| 组件 | 职责 | 关键数据 | 更新路径 |
|---|---|---|---|
| `TransformComponent` `inc/hgl/ecs/components/TransformComponent.h:38-193` | 空间变换（SOA 存储 + 层级） | `storageHandle`（TransformDataStorage）、`parent_id/child_ids`、`cachedWorldMatrix+matrixDirty`、`Mobility`、`fixed_pixel_*` | Setter → `TouchChange(mask)`/`version++`；`TransformSystem` 消费 dirty |
| `MaterialComponent` `inc/hgl/ecs/components/MaterialComponent.h:14-82` | 材质运行期状态（程序 + 两类行地址） | `program`、`data_index_row`、`material_row_cpu/gpu`、`material_texture_*`、`program_dirty/runtime_dirty/valid`、各 hash、`last_materialize_epoch` | `RenderPrimitiveCollectSystem` 解析/物化时写；`MarkValid/MarkProgramResolved/MarkResourcesPending/MarkFailed` 状态机 |
| `PrimitiveComponent` `inc/hgl/ecs/components/PrimitiveComponent.h:47-232` | 「画什么」+ 作者侧资源 | `primitiveAsset`、`runtime_data_buffer/runtime_draw_range`、`namedMaterialTextureResources`、`materialDataResource`、`material_authored_generation`、解析后的 pipeline/renderPass | 设置类 API 一律 `++material_authored_generation` + `InvalidateResolvedRuntimePipeline()`；`EnsureRuntimeGeometryBinding` 在首次解析时建运行时几何绑定 |
| `VisibilityComponent` `inc/hgl/ecs/components/VisibilityComponent.h:15-38` | 可见性开关，**直写共享 storage** | `visible` + `VisibilityDataStorage*` | `SetVisible` 直接更新 storage（`VisibilityDataStorage` 用 `unordered_dense::set<EntityID>` + 互斥量，支持祖先链查询 `IsInvisible`） |
| `BoundingBoxComponent` `inc/hgl/ecs/components/BoundingBoxComponent.h:26-238` | 剔除用的 AABB（SOA） | `storageHandle`（静态共享 BoundingBoxDataStorage）、`worldAABB + valid` | `SetAABB` → `TouchChange`；`BoundingBoxUpdateSystem`/`LineBoundsUpdateSystem` 写 world AABB |
| `CameraComponent` `inc/hgl/ecs/components/CameraComponent.h` | 相机参数与控制模式 | `control_mode`(ViewModel 等)、`target/distance/yaw/pitch`、`camera_data/camera_info/viewport_info`、`matrix_dirty` | `CameraSystem` 每帧按模式计算→`UpdateMatrices` |
| `RenderableComponent` `inc/hgl/ecs/components/RenderableComponent.h:18-44` | 可渲染基类（`PrimitiveComponent` 的查询基） | `visible`、`boundingRadius` | 查询基：`GetComponents<RenderableComponent>()` 召回派生 |
| `LinesComponent` / `TextComponent` | Line / Text 元素数据 | 局部包围盒等 | 各自 Collect/Build 系统 |

组件版本机制（`inc/hgl/ecs/core/Component.h:26-27,74-83`）：`version` 单调自增 + `change_mask` 位掩码（`TransformChange{Position,Rotation,Scale,Parent,WorldMatrix,Mobility}`）。`TransformSystem::ShouldUpdateTransform` 就是靠 mask + version 判定是否需要重算（`src/ecs/systems/tick/TransformSystem.cpp:466-485`）。

---

## 6. Tick 系统层详解

| 系统 | phase | 依赖 | 每帧做什么 |
|---|---|---|---|
| `InputSystem` `src/ecs/systems/tick/InputSystem.cpp:24` | `TickInput` | — | 采集输入；`EndFrame()` 由 `Context::Tick` 收尾（`src/ecs/core/Context.cpp:362-365`） |
| `TransformSystem` `src/ecs/systems/tick/TransformSystem.cpp:14` | `TickTransform` | — | `Update`：只遍历 **movable**（:36-81），按 mask/version 决定 `UpdateIfDirty`；`SubmitTransformUpdates`（:151+）：static 脏则整批重算 → `EnsureTransformBuffer` → `RefreshHandleOrder` → `EnsureCapacity(static,dynamic)` → 静态段/移动段 ring 分开写 → `MarkDirtyRanges` |
| `CameraSystem` `src/ecs/systems/tick/CameraSystem.cpp:222-226` | `TickCamera` | `InputSystem`、`TransformSystem` | `CollectCameras`→`CollectInput`→`ProcessInput`→`UpdateBasis/Transform`→`UpdateMatrices`（写 `camera_data`+`camera_info`，`matrix_dirty=false`，`camera_ubo_dirty=true`，:517-562）；`CommitCameraUBO`（:331-342）**无条件全量写** view 三件套 |
| `VisibilitySystem` `src/ecs/systems/tick/VisibilitySystem.cpp:13-59` | `TickTransform` | — | 构造 `VisibilityDataStorage`；`Initialize` 时把 storage 塞给所有已存在的 `VisibilityComponent` 并同步初值；`Update` 空转（组件直写） |
| `BoundingBoxUpdateSystem` `src/ecs/systems/tick/BoundingBoxUpdateSystem.cpp:15-16` | `TickTransform` | `TransformSystem` | 世界 AABB 维护 |
| `LineBoundsUpdateSystem` `src/ecs/systems/tick/LineBoundsUpdateSystem.cpp:13-59` | `TickTransform` | `TransformSystem` | 线的局部包围盒 → 建/更新 `BoundingBoxComponent`（局部 + 世界 AABB） |

`TransformSystem` 的两段结构是本层最容易被误读的地方：**Tick 阶段只处理 movable 的 CPU 侧 dirty**，static 与全部 GPU 侧（静态段重写 + movable ring 段填充）都在 `SubmitTransformUpdates()` 里，而它由渲染图的 pass 在执行前调用（`src/ecs/core/RenderGraph.cpp:157-161`）。SimpleSphere 的球是 `Mobility::Movable`，所以每帧都进 ring 段。

---

## 7. 渲染系统层详解（以 Primitive 组为主线）

### 7.1 三组相位与系统

| phase | Primitive 组 | Text 组 | Line 组 | 其它渲染系统 |
|---|---|---|---|---|
| `RenderPreBeginFrame` | — | — | — | `EnvironmentSystem`（`src/ecs/systems/render/EnvironmentSystem.cpp:14`）、`ColorPaletteSystem`（`src/ecs/systems/render/ColorPaletteSystem.cpp:17`）、`RenderTargetSystem`（`src/ecs/systems/render/RenderTargetSystem.cpp:12`） |
| `RenderSwapchainNextImage` | — | — | — | `SwapchainNextImageSystem`（`src/ecs/systems/render/SwapchainNextImageSystem.cpp:11`） |
| `RenderBeginFrame` | — | — | — | （`RenderSystemCore::BeginFrame`，非 System） |
| `RenderCollect` | `RenderPrimitiveCollectSystem`（`src/ECS/systems/render/RenderPrimitiveCollectSystem.cpp:379`）+ `PrimitiveCullSystem`（`src/ecs/support/primitive/PrimitiveCullSystem.cpp:10`）+ `PrimitiveSortSystem`（`src/ecs/support/primitive/PrimitiveSortSystem.cpp:10`） | `TextCollectSystem`（`src/ecs/support/text/TextCollectSystem.cpp:10`） | `LineCollectSystem`（`src/ecs/support/line/LineCollectSystem.cpp:11`） | — |
| `RenderBatch` | `PrimitiveBuildSystem`（`src/ecs/support/primitive/PrimitiveBuildSystem.cpp:10`） | `TextBuildSystem`（`src/ecs/support/text/TextBuildSystem.cpp:10`）、`TextSyncSystem`（`src/ecs/support/text/TextSyncSystem.cpp:10`） | `LineBuildSystem`（`src/ecs/support/line/LineBuildSystem.cpp:12`） | — |
| `RenderBufferCommit` | — | — | — | `ViewUBOCommitSystem`（`src/ecs/systems/render/ViewUBOCommitSystem.cpp:15`） |
| `RenderBufferUpload` | — | — | — | `RenderBufferUploadSystem`（`src/ecs/systems/render/RenderBufferUploadSystem.cpp:15`） |
| `RenderFrameSync` | — | — | — | `RenderFrameUBOSyncSystem`（`src/ecs/systems/render/RenderFrameUBOSyncSystem.cpp:13`）、`RenderSceneUBOSystem`（`src/ecs/systems/render/RenderSceneUBOSystem.cpp`） |
| `RenderDrawSubmit` | `PrimitiveRenderSystem`（`src/ecs/support/primitive/PrimitiveRenderSystem.cpp:30`） | `TextRenderSystem`（`src/ecs/support/text/TextRenderSystem.cpp:12`） | `LineRenderSystem`（`src/ecs/support/line/LineRenderSystem.cpp:13`） | — |
| `RenderDebug` | `PrimitiveOverlayRenderSystem`（`src/ecs/support/primitive/PrimitiveOverlayRenderSystem.cpp:26`） | — | — | — |
| `RenderStat` | — | — | `LineStatsSystem`（`src/ecs/systems/render/LineStatsSystem.cpp:12`） | — |
| `RenderSubmit` | — | — | — | `SwapchainSubmitSystem`（`src/ecs/systems/render/SwapchainSubmitSystem.cpp:14`，依赖三个 RenderSystem） |

系统间显式依赖：`TextCollectSystem → RenderPrimitiveCollectSystem`、`TextSyncSystem → TextBuildSystem`、`TextRenderSystem → TextSyncSystem/PrimitiveRenderSystem/RenderBufferUploadSystem`、`LineBuildSystem → LineCollectSystem`、`LineRenderSystem → LineBuildSystem/RenderBufferUploadSystem`、`RenderFrameUBOSyncSystem → RenderBufferUploadSystem/EnvironmentSystem/RenderTargetSystem`、`SwapchainSubmitSystem → Primitive/Text/LineRenderSystem`。

### 7.2 薄代理系统 + pipeline 的双层结构

`RenderPipelineSystem` 家族把「相位 × 元素类型」标准化（`inc/hgl/ecs/support/RenderPipelineSystem.h:24-153`）：

```
RenderPipelineSystem::ValidatePipeline(context)     src/ecs/support/RenderPipelineSystem.cpp:6-19
   = IsEnabled() && context && GetPipeline(context)!=nullptr
CollectSystem::Update  final → OnCollect(pipeline)     :24-31
CullSystem::Update     final → OnCull                  :36-43
SortSystem::Update     final → OnSort                  :48-55
BuildSystem::Update    final → OnBuild                 :60-67
SyncSystem::Update     final → OnSync                  :72-79
RenderPipelineDrawSystem::Render final → OnRender(pipeline, cmd)   :84-91
```

于是具体系统只剩三步样板（`src/ecs/support/primitive/PrimitiveCullSystem.cpp:7-22` 为例，全文件 23 行）：构造里 `SetExecutionPhase(...)` + `SetRenderElementType("Primitive")`；`GetPipeline()` 返回 `context->GetRenderPipeline("Primitive")`；`OnCull` 转调 `pipeline->RunCull()`。**Update/Render 被 `final` 封死**——想插入逻辑必须经 pipeline 或新加阶段基类。

pipeline 侧接口 `RenderPipelineBase`（`inc/hgl/ecs/support/RenderPipelineBase.h:16-56`）：`PrepareFrame/RunCollect/RunCull/RunSort/RunBuild/RunSync/Render`。`PrimitiveRenderPipeline` 只是把请求转给 `PrimitiveBatchPipeline` 实现体（`src/ecs/support/primitive/PrimitiveRenderPipeline.cpp:8-63`）：

```
RunCull  → impl_->PrepareFrame() + RunCulling()
RunSort  → impl_->PrepareFrame() + RunSorting()
RunBuild → impl_->PrepareFrame() + RunTransformIndexing() + RunBatching()
Render   → 空实现（绘制由 PrimitiveRenderSystem 直读帧缓存发给 PipelineMaterialRenderer）
```

`PrepareFrame`（`src/ecs/support/PrimitiveBatchPipeline.cpp:84-106`）是幂等守门：确认 `cache.renderItems` 非空、取 cameraInfo/device、对齐帧序号——所以 Cull/Sort/Build 三个系统各自调都没问题。

### 7.3 收集阶段：RenderPrimitiveCollectSystem

`Update`（`src/ECS/systems/render/RenderPrimitiveCollectSystem.cpp:1091-1385`）是本层最重的系统：

1. **帧缓存开帧**：`cache.BeginFrame()` 清 renderItems、清零计数、调各 `MaterialBatch::Clear()` 但保留 batch 对象复用（`src/ecs/core/Context.cpp:40-51`）；
2. **取可见性 storage**（O(1) 跳过不可见实体）；
3. **预扫描 + 物化 epoch**（:1122-1208）：判定本帧是否有任何 primitive 需要材质工作；若有需要且存在「可能用到 runtime rows」的 primitive，则 `++materialize_epoch`（epoch 语义：物化会重写共享行表，被跳过的 primitive 下次必须重物化）；
4. **主循环**（:1217-1375）：过滤 `!IsVisible / !CanRender / storage 不可见 / 无 owner / 无 recipe` → 拿或建 `MaterialComponent` → 三条路径：
   - 全清帧：只跑轻量快速路径（`ResolveMaterialProgramForPrimitive` + `EnsureRuntimeGeometryFromAsset` + `ResolveRuntimePipelineForPrimitive`），成功 `MarkValid`；
   - 需要工作帧：`PrepareActivePlanResources` → `MaterializeRecipeRowsForPrimitive` → `EnsureRuntimeGeometryFromAsset` → `ResolveRuntimePipelineForPrimitive` → `MarkValid`；任一步失败 `InvalidateRecipeRuntime` + `MarkFailed`（下一帧重试）；
5. **生成 RenderItem**：`make_unique<PrimitiveRenderItem>(entity_id, transform, primitiveComp, materialComp, world)`，填 `worldPosition/distanceToCamera`（供排序），`UpdateWorldMatrix()`，push 进 `cache.renderItems`，`renderableCount++`（:1362-1374）。

`PrimitiveRenderItem`（`inc/hgl/ecs/core/PrimitiveRenderItem.h:28-69`、`src/ecs/core/PrimitiveRenderItem.cpp:12-30`）是 `RenderItem` 接口的实现：对外暴露实体/组件句柄、`GetShaderProgram/GetPipeline/GetGeometryDataBuffer/GetGeometryDrawRange`（后两者转问 `PrimitiveComponent` 的运行时绑定），并在构造时缓存世界矩阵与世界坐标。批处理只需要 `RenderItem*` 这层接口——这是 ECS 与渲染层解耦的接缝。

### 7.4 批处理阶段：PrimitiveBatchPipeline

| 步骤 | 函数 | 内容 |
|---|---|---|
| 剔除 | `RunCulling → PerformFrustumCulling` `:141-233` | 视锥测试（`TestFrustumWithWorldAABB / LocalAABB / BoundingSphere`），置 `item->isVisible` |
| 排序 | `RunSorting → SortByDistance` `:219-231` | 按 `distanceToCamera` |
| 变换索引 | `RunTransformIndexing → AssignTransformIndices` `:232-302` | 由 handle 查组内序号，movable → `dynamic_base + group_index`，static → `group_index + 1`（slot 0 恒为单位矩阵）；写 `item->transform_index` |
| 建成批 | `RunBatching → BuildMaterialBatches + FinalizeBatches` `:894-983` | 按 `ShaderProgramPipelineKey{shader_program, pipeline}`（`inc/hgl/ecs/core/ShaderProgramPipelineKey.h:23-46`）归批；缺 recipe rows 或未解析 pipeline 的 item 直接跳过并告警 |
| 批内定型 | `FinalizeBatch` `:639-646` | `SortBatchItems` → `EnsureBatchIndexRows` → `EnsureMeshDrawParams` → `BuildBatches`（生成 DrawBatch，同几何同 range 合并实例，:354-450）→ `WriteBatchIndexRows`（写 L2W 索引行 + `MaterialInstanceAddresses` 行，:786-893） |

`MaterialBatch`（`inc/hgl/ecs/core/MaterialBatch.h:40-84`）是「一个 shader program 的全部待绘项」：`key`（shader+pipeline）、`items`（RenderItem*）、`draw_batches`、`mesh_draw_params_buffer`、`l2w_index_buffer`、`material_data_index_rows_buffer`、`icb_mesh_tasks`（间接命令）、`transform_buffer`（非拥有，来自 TransformSystem）、`renderer`（`PipelineMaterialRenderer`）。

### 7.5 提交阶段：从帧缓存到 vkCmd

`PrimitiveRenderSystem::OnRender`（`src/ecs/support/primitive/PrimitiveRenderSystem.cpp:39-93`）：从 `cache.materialBatches` 收集非空批 → 按 `MaterialBatch::key` 排序 → 跳过 `pipeline->GetOverlay()` 的批（交给 Overlay 系统）→ 逐个 `batch->renderer->Render(cmd, draw_batches, count, transform_buffer, batch, render_context)`。`PipelineMaterialRenderer` 的批次内动作见配套文档 `doc/simple-sphere-ecs-render-chain.md` §5。

---

## 8. ECS 与 GPU 的接口面（谁写哪块 GPU 数据）

| GPU 数据 | 写入者（ECS 侧） | 载体 |
|---|---|---|
| L2W 矩阵（静态段 + movable ring 段） | `TransformSystem::SubmitTransformUpdates`（tick 系统，渲染前被图调用） | `TransformAssignmentBuffer`（`src/ecs/support/TransformAssignmentBuffer.cpp`） |
| L2W 索引行 / MaterialInstanceAddresses 行 | `PrimitiveBatchPipeline::WriteBatchIndexRows`（RenderBatch 相位） | `MaterialBatch::l2w_index_buffer` / `material_data_index_rows_buffer` |
| MeshDrawParams 行 + mesh 间接命令 | `PrimitiveBatchPipeline::WriteMeshDrawCommands` | `MaterialBatch::mesh_draw_params_buffer` / `icb_mesh_tasks` |
| 材质参数行（PBRSurfaceRow…） | 作者侧 `MaterialSSBODataAccessor::Write`；地址由 RPCS `MaterializeRecipeRowsForPrimitive` 解析 | `MaterialSSBOBufferRegistry` 的 `ActiveRowPool` |
| 纹理引用行（uvec2 handle/layer） | RPCS 材质化（`:960-1044`） | `MaterialTextureReferencePool`（按 definition 一池） |
| CameraInfo / SkyInfo / ViewportInfo UBO | `CameraSystem::CommitCameraUBO`（RenderBufferCommit）+ `ViewUBOCommitSystem`；挂载由 `RenderSceneUBOSystem::ApplyResourceLayoutBindings` | 全局 Scene 描述符集 |
| 脏 buffer 的 GPU 传输 + barrier | `RenderBufferUploadSystem::Update`（遍历 `device->GetGPUBufferRegistry()`） | `IGPUBuffer::CopyToDevice` + `vkCmdPipelineBarrier` |
| swapchain 图获取 / 提交 | `SwapchainNextImageSystem`（帧首）/ `SwapchainSubmitSystem`（帧尾，依赖三个 Render 系统） | `SwapchainRenderTarget::NextFrame/Submit` |

---

## 9. 帧缓存与生命周期

- `RenderFrameCache`（`inc/hgl/ecs/core/Context.h:49-63`）：`renderItems`（unique_ptr 数组，每帧重建）、`materialBatches`（key→`MaterialBatch` 的 map，**跨帧保留对象**只 `Clear()` 内容）、`cameraInfo`、`renderableCount`；
- 跨帧存活的还有：`TransformAssignmentBuffer`（TransformSystem 持有）、`MaterialBatch` 的各类 buffer（`Ensure*` 按 pow2 扩容）、行池（registry 持有）；
- 非拥有关系要小心：`MaterialBatch::transform_buffer` 是 TransformSystem 的指针（`inc/hgl/ecs/core/MaterialBatch.h:67` 注释明示勿跨帧缓存系统指针）；`PrimitiveRenderItem` 持 `shared_ptr` 组件句柄（会延长组件寿命到当帧结束）；
- `frame_index`（`SetFrameIndex`，`src/ecs/core/Context.cpp:1192-1196`）供 ring 分段；`render_submission_serial` 供资源回收（纹理引用池的 retire 延迟与渲染层帧在飞数挂钩）。

---

## 10. 可观测性

| 手段 | 位置 / 形态 |
|---|---|
| 系统计时 | `SystemProfiler`（`inc/hgl/ecs/core/SystemProfiler.h:18-57`）：per-system `last/avg/max ms` + per-group `{componentCount, enabled, ensure/activation/deactivation count}`；由 `RunSystemUpdate` / `RunRenderSystemsInRange` 自动包裹 |
| 遍历日志 | `[ECS] Update Begin/End: <System>`（`src/ecs/core/Context.cpp:775,783`）、`[ECS] Render Begin/End: <System> (phase N)`（`src/ecs/core/Context.cpp:805,813`） |
| 帧边界 | `[ECS RENDER] ===== Frame Start/End (RenderGraph ...) =====`（`src/ecs/core/RenderGraph.cpp:180,188`）、`Executing pass N (phases a-b)`（`src/ecs/core/RenderGraph.cpp:125-126`） |
| 组状态 | `SystemGroupRegistry::DebugPrint`（首帧注册组时打印，`src/ecs/core/RenderGraph.cpp:83-86`）；脏帧才建图 |
| 材质/寻址诊断 | `ULRE_ARENA_DEBUG` 环境变量开启 `[ArenaDebug]/[ArenaTrace]/[MaterialTextureReferences]` 日志（RPCS） |

---

## 11. SimpleSphere 的 ECS 映射

| 示例代码 | ECS 对象 | 驱动者 | 产出 |
|---|---|---|---|
| `CreateEntity<Entity>("SphereEntity")` | Entity + EntityID | `Context::CreateEntity` | EntityManager 表项 |
| `AddComponent<TransformComponent>(Mobility::Movable)` | TransformComponent | `RegisterComponentInstance`（建 TransformComponent 时另走 `RegisterTransformComponent` 进 movable 列表） | movable_transforms 一项 |
| `SetLocalPosition/Rotation` + 每帧 `Tick` 自转 | 同上（mask/version 变化） | `TransformSystem::Update` → `SubmitTransformUpdates` | L2W 矩阵写入 movable ring 段 |
| `AddComponent<PrimitiveComponent>()` + `SetPrimitiveAsset/SetMaterialTextureResource/SetMaterialDataResource` | PrimitiveComponent（`material_authored_generation++`） | 反向注册 → 自动安装并打开 **"Primitive" 组** | 组激活 + 系统注册 |
| `sphere_recipe`（Lit/PBR） | 挂在 `PrimitiveAsset` 上 | `RenderPrimitiveCollectSystem` 解析/物化 | `MaterialComponent.program` + `material_row_gpu` + `material_texture_row_gpu` |
| `CreateCamera` + `CameraComponent{ViewModel}` | CameraComponent | `CameraSystem` | `camera_info` → 全局 Scene UBO |
| 球体几何（私有 VAB/IBO） | `PrimitiveComponent::runtime_data_buffer`（GeometryDataBuffer） | RPCS `EnsureRuntimeGeometryFromAsset` | 三流 `addr_*` 写进 MeshDrawParams 行 |

单实体场景下 ECS 的实际执行面：**1 个 Entity、3 个组件**（Transform/Primitive/Material，MaterialComponent 由收集系统自动补建）、**1 个 MaterialBatch、1 个 DrawBatch、1 行参数、1 行材质地址、1 行纹理引用**——日志 `[IndirectMeshDraw] mesh indirect flush engaged: first=0 count=1` 即这一行的结果。

---

## 12. 扩展点与雷区

1. **不要手写遍历顺序**：顺序 = `ExecutionPhase` + 显式依赖（`AddDependency<T>`）+ 注册先后；想插到某系统前就加依赖，别改 phase 数值。
2. **Update 与 Render 是两套遍历**（§4.4）：`RenderCollect/RenderBatch/...` 阶段的工作写在 `Update()` 里，pass 内录命令写在 `Render()` 里；把录命令逻辑写进 `Update()` 会在 cmd buffer 未开时崩。
3. **`final` 封死的阶段基类**：要扩展 Collect/Build/Render 的行为，必须经对应 pipeline（`RenderPipelineBase` 虚函数）或新增阶段基类，不能直接 override。
4. **组开关是双向的**：组件计数归零 → `SetElementTypeSystemsEnabled(false)` 关掉整组系统；因此「系统没跑」的第一嫌疑是**组里没有组件**（而不是系统没注册）。新增元素类型必须同时提供：组件（`GetSystemGroupName`）+ 组安装器（`DefaultSystems`）+ 相位区间可推导的系统。
5. **图构造依赖组元数据**：`EnsureSystemGroupsRegistered` 必须先于建图（`src/ecs/core/DefaultSystems.cpp:244` 已保证）；运行期新出现的元素类型会让 `MarkSceneStructureDirty` 生效，adaptive 图才会重建。
6. **帧缓存生命周期**：`cache.renderItems` 每帧重建（持有对象的是 unique_ptr），`materialBatches` 跨帧复用但每帧 `Clear()`——**不要跨帧缓存 `RenderItem*`**；`MaterialBatch::transform_buffer` 是非拥有指针。
7. **物化 epoch 语义**：共享行表被重写会让「本帧被跳过的 primitive」行号失效，因此 `materialize_epoch` 不匹配时必须重物化（RPCS `:1122-1208` 的预扫描就是为此）。改收集逻辑时保留这条不变量。
8. **静态/移动双通道**：`Mobility::Static` 的 GPU 写只在 `SubmitTransformUpdates` 里按需整批重写，`Movable` 每帧进 ring 段。把大量常驻对象设成 Movable 会每帧付出全量 ring 写代价。
9. **注册阶段两道门**：子世界/根世界的 scope 门（`inc/hgl/ecs/core/Context.h:524-595`）在示例外（多世界）场景会静默拒绝注册，只看 `rejected_render_system_registration_count`/首条告警能发现。
10. **诊断开关**：`ULRE_ARENA_DEBUG`（材质/寻址）、`SystemProfiler`（谁慢）、`[ECS] Render Begin: X (phase N)`（谁在跑）——排查「某系统没生效」按这三层依次排除：组是否启用 → 系统是否注册/启用 → 阶段是否被 pass 覆盖。
