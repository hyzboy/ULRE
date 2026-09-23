# ULRE ECS 层架构与帧流程详解（技术文档）

> 基线：**2026-09-24，分支 `CSM`**——描述符机制退役后的 BDA 终态：执行相位表、系统/系统组模型、4-ID 图元描述符与全局 SSBO 行池。trace 载体 = `example/Basic/SimpleSphere.cpp`（单球体 + `Lit` 材质，一个 Primitive 实体 + 一个 Camera 实体）。
> 范围：`inc/hgl/ecs/` + `src/ecs/` 全部分层（core / components / systems(tick,render) / support），以及 ECS 与渲染层（Graph/Vulkan）的接口面。
> 与 `doc/simple-sphere-ecs-render-chain.md` 的分工：那篇讲「数据怎么从作者 API 走到 vkCmd」；本篇讲「ECS 层内部由谁、在什么阶段、按什么顺序驱动这些数据」。
> 全部 path:line 在本基线核实；**符号名是稳定锚点，行号随重构漂移**。本文只描述现状。

---

## 1. 分层与两个循环

```
inc/hgl/ecs/
├ core/        Object / Component / Entity / EntityHandle / EntityManager / Context
│              System / SystemGroup / RenderGraph / RenderPassRequest
│              RenderItem / PrimitiveRenderItem / InstancedPrimitiveRenderItem
│              MaterialBatch / ShaderProgramPipelineKey
├ components/  Transform / Primitive / InstancedPrimitive / Material / Camera
│              Visibility / BoundingBox / Lines / Text / Renderable
├ systems/
│  ├ tick/     InputSystem / TransformSystem / CameraSystem (+ CameraInputMapping)
│  │           VisibilitySystem / LineBoundsUpdateSystem
│  └ render/   RenderPrimitiveCollectSystem / RenderBufferUploadSystem
│              RenderSceneUBOSystem / ViewUBOCommitSystem / RenderSystemCore
│              RenderTargetSystem / SwapchainNextImageSystem / SwapchainSubmitSystem
│              EnvironmentSystem / ColorPaletteSystem / LineStatsSystem
└ support/     RenderPipelineBase / RenderPipelineSystem / PrimitiveBatchPipeline
               PipelineMaterialRenderer / PrimitiveRenderPipeline / LineRenderPipeline
               TextRenderPipeline / RenderResource
               TransformAssignmentBuffer / TransformDataStorage / VisibilityDataStorage
               BoundingBoxDataStorage / RenderItemDataStorage / DrawItemIDStorage
               DrawItemCompaction / PositionSourceSpec / TransformPolicySpec
```

两个互相独立的循环（帧入口 `src/Work/WorkManager.cpp:74`）：

| 循环 | 入口 | 遍历对象 | 内容 |
|---|---|---|---|
| **Tick 循环** | `ECSContext::Tick` `src/ecs/core/Context.cpp:290-314` | `tick_system_order` | tick 系统 `Update()` 顺序执行 → 末尾 `InputSystem::EndFrame()`（:310-313）。**每帧全实体 × 全组件的 `OnUpdate` 虚分发已删除**（:304-308 注释明示：逻辑更新归 tick 系统，不归组件） |
| **Render 循环** | `ECSContext::Render(dt, pre_render)` `src/ecs/core/Context.cpp:598-618` | `render_system_order`（按相位分段） | 结构脏时重算图 → `BeginManagedRenderFrame` → `ExecuteRenderGraphPasses` → `EndManagedRenderFrame`（见 §4） |

两者**共用一套系统注册表与拓扑排序**，靠 `ExecutionPhase` 的数值分界自动分派（见 §3.1）。

---

## 2. 核心对象模型

### 2.1 对象三段式

| 类 | 基类 | 身份 | 生命周期动词 | 关键成员 |
|---|---|---|---|---|
| `Object` | — | `objectId` + `objectName` | **无虚生命周期钩子**（`Object::OnUpdate` 链已删除） | `std::string objectName` `inc/hgl/ecs/core/Object.h:15-60`，注释 :54-59 明示三套动词体系 |
| `Component` | `std::enable_shared_from_this<Component>` | 归属 `EntityID owner_id` + `ECSContext* owner_context` | `OnAttach / OnDetach` | `componentName` / `version`（:26）/ `change_mask`（:27）；`GetSystemGroupName()` 默认返回 `nullptr` `inc/hgl/ecs/core/Component.h:19-84` |
| `Entity` | `Object` | `EntityID id`（由 EntityManager 分配） | 无自有钩子（挂载/卸载转发给组件） | `UnorderedMap<size_t, shared_ptr<Component>> components`（键 = `typeid(T).hash_code()`）`inc/hgl/ecs/core/Entity.h:21-102` |
| `System` | `Object` | `typeid(T).hash_code()` 作为注册键 | `Initialize / Update / Render / Shutdown` | phase / dependencies / enabled / `render_element_type` `inc/hgl/ecs/core/System.h:55-117` |

生命周期动词是**三套不同语义**，不要统一（`inc/hgl/ecs/core/Object.h:54-59` 注释明示）：Component 挂载卸载 / System 初始化运行；Object 层只保留构造函数与析构。`System` **没有** `OnCreate` / `OnDependenciesReady`（全仓 0 命中）；`CreateEntity` 也不触发实体级 `OnCreate`。

### 2.2 组件挂载 → 自动反向注册

`AddComponent<T>(args...)`（`inc/hgl/ecs/core/Entity.h:54-61`）→ `Entity::ReplaceComponent`（`inc/hgl/ecs/core/Entity.h:32`）：

```
components[typeid(T).hash_code()] = component
SetOwner(entity_id, context)           // 组件持回链，可 GetOwnerID()/GetOwner()
RegisterToContext(...) → ECSContext::RegisterComponentInstance   ← 关键：向 Context 反向登记
component->OnAttach()
MarkSceneDirty() → context->MarkSceneStructureDirty()            ← 触发下一帧重算图
RemoveComponent<T>() 对称：UnregisterFromContext → OnDetach → 删除 → MarkSceneDirty（Entity.h:81-93）
```

`ECSContext::RegisterComponentInstance`（`src/ecs/core/Context.cpp:1125-1153`）做三件事：

1. 调 `RegisterComponentInstanceInternal`（`src/ecs/core/Context.cpp:1189-1212`）把组件按类型 hash 存进 `component_registry`（`weak_ptr` 列表）——`GetComponents<T>` 就是遍历这张表（`inc/hgl/ecs/core/Context.h:483-501`）；
2. 若组件声明了 `GetSystemGroupName()`（`PrimitiveComponent` 返回 `"Primitive"`，`inc/hgl/ecs/components/PrimitiveComponent.h:159`）：**自动安装该组的系统**（`EnsureSystemGroupSystems`）+ **打开该组系统开关**（`SetElementTypeSystemsEnabled(group, true)`）；
3. 对 `component_query_bases` 再做一次登记，于是 `GetComponents<RenderableComponent>()` 能召回派生子类实例。当前登记基类的入口是 `RegisterComponentQueryBase<PrimitiveComponent>()`（`src/ecs/core/Context.cpp:88`，模板定义 `inc/hgl/ecs/core/Context.h:502-514`）。

反注册对称处理：`UnregisterComponentInstance`（`src/ecs/core/Context.cpp:1154-1187`）在计数归零时关组。**没有** `DisableUnusedSystemGroups` / `CleanupSystemGroup` 这类显式入口（全仓 0 命中）——组开关的唯一驱动点是 `CreateAdaptiveRenderGraph` 按本世界 `SceneStats.active_render_groups` 逐组调用（`src/ecs/core/RenderGraph.cpp:230-237`）。

### 2.3 Entity 管理

`EntityManager`（`inc/hgl/ecs/core/EntityManager.h`、`src/ecs/core/EntityManager.cpp`）持全部 Entity（`ECSContext` 构造时预分配 **1000**，`src/ecs/core/Context.cpp:60-68`），EntityID = 句柄（`inc/hgl/ecs/core/EntityHandle.h`）。创建走 Context 模板：

```
CreateEntity<T>(args...)                       inc/hgl/ecs/core/Context.h:369-380
  → make_unique<T> → entity_manager->CreateEntity → SetContext(this) → 存入实体表
GetAllEntities(out)                            inc/hgl/ecs/core/Context.h:410
```

`Context::CreateChildEntity(parent, desc)` 这类 API **不存在**（全仓 0 命中；只有 `src/SceneGraph/gizmo/GizmoUnified.AssetVisual.inl` 里的局部助手 `CreateChildEntityWithTransform`，非 ECSContext API）。层级用 `TransformComponent` 自带 API 组装（`inc/hgl/ecs/components/TransformComponent.h:145-154`）：`SetParent(EntityID)` / `GetParent()` / `AddChild` / `RemoveChild` / `GetChildren()`。

`TransformComponent` 有专门的旁路索引：`Context::RegisterTransformComponent`（`src/ecs/core/Context.cpp:1213-1242`）、`MigrateTransformComponent`（:1243-1298）、`UnregisterTransformComponent`（:1299-1320）维护 `static_transforms` / `movable_transforms` 两个 `weak_ptr` 向量（`inc/hgl/ecs/core/Context.h:118-121`）——Tick 系统靠它们做到「只遍历 movable」。

---

## 3. 系统模型与阶段模型

### 3.1 ExecutionPhase：全引擎唯一的时间轴

`inc/hgl/ecs/core/System.h:22-48`（枚举值即顺序，共 **15 项**）：

| # | 阶段 | 语义 |
|---|---|---|
| 0 | `TickInput` | 输入 |
| 1 | `TickTransform` | 变换 / 包围盒 / 可见性 |
| 2 | `TickCamera` | 相机矩阵 |
| 3 | `TickPostCamera` | 面向策略变换（gizmo 像素缩放等） |
| 4 | `RenderSwapchainNextImage` | 取 swapchain 图（cmd 尚未开） |
| 5 | `RenderPreBeginFrame` | 每帧环境 / viewport 同步（cmd 尚未开） |
| 6 | `RenderCollect` | 收集 / 剔除可见组件（cmd 已开、pass 外） |
| 7 | `RenderBatch` | 建批、写行表与命令表 |
| 8 | `RenderBufferCommit` | 收尾 CPU 写（相机/视口 UBO） |
| 9 | `RenderBufferUpload` | GPU 传输 + barrier |
| 10 | `RenderFrameSync` | 上传后同步场景 UBO/描述符 |
| 11 | `RenderDrawSubmit` | pass 内录绘制命令 |
| 12 | `RenderDebug` | pass 内调试叠加 |
| 13 | `RenderStat` | pass 后统计 |
| 14 | `RenderSubmit` | 提交呈现 |

**不存在** `RenderResourceSetup` / `RenderMaterialBind` / `RenderBeginFrame` / `RenderPostProcess`（4 名全仓 0 命中）——曾经用数字（9/10/11/12/13/14/16）指代的做法已废弃，一律写枚举名。

分界值 = `RenderSwapchainNextImage`：`AddOrUpdateSystem` 用它判定「tick 系统 or render 系统」（`src/ecs/core/Context.cpp:1008`），注册方传错会被自动纠正。

### 3.2 系统注册

```
RegisterSystem<T>(is_render, args...)   inc/hgl/ecs/core/Context.h:422-429
RegisterTickSystem<T>(args...)          inc/hgl/ecs/core/Context.h:433-436
RegisterRenderSystem<T>(args...)        inc/hgl/ecs/core/Context.h:440-443
GetSystem<T>()                          inc/hgl/ecs/core/Context.h:447-457（先查 tick_systems 再查 render_systems）
EnsureTickSystem<T> / EnsureRenderSystem<T>    src/ecs/core/DefaultSystems.cpp:39-62（幂等）
```

**注册没有权限门**：`SystemOwnershipScope` / `ContextRole` / `RegisterTickSystemScoped` / `RegisterRenderSystemScoped` / `rejected_render_system_registration_count` 全部 0 命中（多世界子系统权限门已退役）。`AddOrUpdateSystem`（`src/ecs/core/Context.cpp:999-1075`）的职责收敛为：

- 按相位决定进 `tick_systems` 还是 `render_systems`；
- 非空 `render_element_type` 时把系统登记进 `systems_by_element_type[type]`——这是「整组开关」的抓手；
- 维护 `OrderedSystem{key, phase, insertion_order, system}` 列表（`next_system_order` 单调递增，`inc/hgl/ecs/core/Context.h:102`），置 `*_order_dirty`；
- 把 `system->GetDependencies()` 声明的依赖写进依赖图（`AddSystemDependency`，`src/ecs/core/Context.cpp:1076-1098`）；
- 若 Context 已 active，立即 `Initialize()`（运行期热插拔系统）。

依赖声明方式唯一：`System::AddDependency<T>()`（`inc/hgl/ecs/core/System.h:110-115`）。当前代码里一共 4 处：
`LineBoundsUpdateSystem → TransformSystem`（`src/ecs/systems/tick/LineBoundsUpdateSystem.cpp:14`）、`TextCollectSystem → RenderPrimitiveCollectSystem`（`src/ecs/support/text/TextCollectSystem.cpp:12`）、`TextSyncSystem → TextBuildSystem`（`src/ecs/support/text/TextSyncSystem.cpp:12`）、`TextRenderSystem → PrimitiveRenderSystem`（`src/ecs/support/text/TextRenderSystem.cpp:14`）。

### 3.3 拓扑排序 = 相位优先 + 插入序稳定 + 环回退

`SortSystemList`（`src/ecs/core/Context.cpp:879-987`）用 Kahn 拓扑排序，**可用集比较器是 `(phase, insertion_order)`**：

- 依赖只表达「必须先于」，相位表达「阶段」，两者同时满足时按注册先后；
- 依赖目标缺失 → 告警（`src/ecs/core/Context.cpp:917`）并跳过该边；
- 出现环 → 打印告警并整体回退 `phase/insertion_order` 稳定排序（:978）；
- 仅在 dirty 时才重排，结果缓存在 `tick_system_order` / `render_system_order`（`SortTickSystems` :869 / `SortRenderSystems` :874）。

### 3.4 初始化顺序

`ECSContext::Initialize(device, target)`（`src/ecs/core/Context.cpp:75-155`，W3 合并后原 `InitializeGraphics` 已并入）：

```
1 绑定 gpu_device / render_target；把 device 传播给已注册的 RenderBufferUploadSystem（:84-86）
2 RegisterComponentQueryBase<PrimitiveComponent>()（:88）
3 确保基础系统存在：TransformSystem(tick, :95) / VisibilitySystem(tick, :109) / RenderBufferUploadSystem(render, :124)
4 SortTickSystems + SortRenderSystems（:133-134）
5 按顺序对每个系统执行 Initialize()：先 tick 序（:137-143）再 render 序（:145-151）
6 active = true（:153）
```

`DefaultSystems.cpp` 再补上「核心 + 三个元素组」：

```
EnsureCoreEcsSystems            src/ecs/core/DefaultSystems.cpp:165-207
  InputSystem / CameraSystem / EnvironmentSystem / ColorPaletteSystem
  RenderTargetSystem / SwapchainNextImageSystem / SwapchainSubmitSystem
  RenderSceneUBOSystem / ViewUBOCommitSystem
  （SetRenderContext / SetViewportInfo / SetRenderTarget 在这里接线）
EnsureSystemGroupSystems(ctx, "Primitive"|"Text"|"Line")   :209-226 → 组安装器
RegisterDefaultEcsSystems       :228-245 → 返回 input_system 给 WorkObject
                                 （由 src/Work/AppFramework.cpp:251 调用）
```

组安装器（`src/ecs/core/DefaultSystems.cpp:64-163`）按名字注册到 `SystemGroupRegistry`，内容是「建 pipeline + 注册该组的薄代理系统」：

| 组 | 安装内容 |
|---|---|
| Primitive | `PrimitiveRenderPipeline` + `RenderPrimitiveCollectSystem`（共享）+ `RenderBufferUploadSystem`（共享）+ Cull/Sort/Build/Render/OverlayRender |
| Text | `TextRenderPipeline` + Collect/Build/Sync/Render |
| Line | `LineRenderPipeline` + `LineCollectSystem/LineBuildSystem/LineRenderSystem` + tick 的 `LineBoundsUpdateSystem` + `LineStatsSystem` |

### 3.5 SystemGroup / 元素类型 / 组开关

- `SystemGroup{name, startPhase, endPhase}`（`inc/hgl/ecs/core/SystemGroup.h:28-43`）——**只有组定义，没有 `enabled` 字段**：组的「是否启用」是每世界状态，由 `CreateAdaptiveRenderGraph` 按本世界 `SceneStats` 即时推导（`src/ecs/core/RenderGraph.cpp:226-240`），不写进全局注册表（多世界建图时全局存会被互相覆盖）。
- 注册表 `SystemGroupRegistry`（`inc/hgl/ecs/core/SystemGroup.h:53-89`）是**静态单例**，只承载真正的全局不变量：组定义 + `RegisterGroupInstaller` 插件式安装器；API 为 `RegisterGroup` / `RegisterGroupInstaller` / `EnsureGroupSystems` / `GetAllGroups` / `GetGroup` / `DebugPrint`。**`ScopedSystemGroupState` 与旧别名 `RenderSystemGroup / RenderSystemGroupRegistry / ScopedGroupState` 均已删除**（0 命中）。
- 组元数据在首帧由 `EnsureSystemGroupsRegistered` 从系统集合反推：遍历每个 element type 的系统，取 `min(startPhase)/max(phase)` 作为组区间（`src/ecs/core/RenderGraph.cpp:21-87`，幂等，只补缺失，注册后打印一条组清单日志）。
- 「组是否启用」的落地点唯一：`SetElementTypeSystemsEnabled` 直接 `SetEnabled(false)` 掉该组所有系统（`src/ecs/core/Context.cpp:1342-1356`）。

对 SimpleSphere：只有 `PrimitiveComponent` 存在 → 只有 `"Primitive"` 组有组件计数 → Text/Line 组的系统在首帧就被关掉；这一点在日志里表现为 pass 只有 1 个、且看不到 Line/Text 系统。

### 3.6 RenderGraph：pass 序列

`RenderGraph::Pass{startPhase, endPhase, renderTarget, enabled, runUpdate, submitTransforms, runRender, onBeforePass, onAfterPass}`（`inc/hgl/ecs/core/RenderGraph.h:30-85`）。**只有一条构造路径**：

| 路径 | 内容 | 触发 |
|---|---|---|
| `CreateAdaptiveRenderGraph(context, stats)`（`inc/hgl/ecs/core/RenderGraph.h:145`，`src/ecs/core/RenderGraph.cpp:217-262`） | 遍历 `SystemGroupRegistry` 全部组：先按 `stats.HasGroup(name)` 调 `SetElementTypeSystemsEnabled`；命中的组各 `Add` 一段 pass（`startPhase..endPhase`，全 true，`renderTarget = nullptr`） | 每帧 `Render` 入口；仅当 `scene_structure_dirty` 且 `SceneStats::GetHash()` 变化才真正重建（`src/ecs/core/Context.cpp:602-615`） |

`CreateDefaultLinearGraph` / `use_adaptive_render_graph` / `SetAdaptiveRenderGraphEnabled` 均已删除（0 命中）。

`SceneStats`（`inc/hgl/ecs/core/RenderGraph.h:112-133`）只持有 `std::set<std::string> active_render_groups` + FNV 风格 `GetHash()`；`GatherSceneStats`（`src/ecs/core/RenderGraph.cpp:179-215`）遍历全部实体及其组件，按 `Component::GetSystemGroupName()` 收敛组名集合。

**注意 `Pass::renderTarget` 尚未生效**：`ExecuteRenderGraphPasses` 不读该字段（cmd buffer 由帧初始 RT 的 `RenderSystemCore::BeginFrame()` 取得），设了非当前 RT 只会打一条告警（`inc/hgl/ecs/core/RenderGraph.h:38-48` 警告注释、`src/ecs/core/RenderGraph.cpp:110-118`）。跨 RT 渲染的现行路径是 `ECSContext::RenderTo(RenderPassRequest)`（§4.5），不是 pass 的 RT 字段。

`EnsureSystemGroupsRegistered` 必须在建图前跑过一次（`src/ecs/core/DefaultSystems.cpp:240`），否则组元数据缺失 → 图里没有对应 pass。

---

## 4. 帧驱动：两遍扫描 + 一次 pass 循环

### 4.1 帧入口

```
ECSContext::Render(dt, pre_render)                        src/ecs/core/Context.cpp:598-618
 └ scene_structure_dirty 时 GatherSceneStats + hash 比较（变化才重建图）
 └ ECSContext::Render(dt, graph, pre_render)              src/ecs/core/RenderGraph.cpp:162-177
    ├ BeginManagedRenderFrame(dt)                         src/ecs/core/Context.cpp:331-381
    │   ① AcquireSwapchainImage()       → 相位 RenderSwapchainNextImage（Context.cpp:342/:638）
    │   ② RenderPreBeginFrame()         → 相位 RenderPreBeginFrame（Context.cpp:350/:620）
    │   ③ SyncRenderTargetViewport()    ← 目标 RT 尺寸与视口不一致时 OnResize（:351/:679）
    │   ④ TextureManager::UpdateUploadQueue(bindless_mgr)（:353-359）
    │   ⑤ render_core->BeginFrame()     → 开 cmd buffer（:362 / RenderSystemCore.cpp:42）
    │   ⑥ SetCurrentRenderCmd(cmd)      ← 后续所有 Render() 都拿它（:368）
    │   ⑦ PrepareRenderPassSetup(...)   ← 见 4.2（:369/:719）
    │   ⑧ render_core->BeginRenderPass(options) → vkCmdBeginRendering（:372 / RenderSystemCore.cpp:79）
    ├ ExecuteRenderGraphPasses(graph, dt, pre_render)     src/ecs/core/RenderGraph.cpp:91-160
    │   └ pre_render(dt)（= WorkObject::OnRenderPass）在**进入 pass 循环前**调用一次（:95-96）
    └ EndManagedRenderFrame(dt)                           src/ecs/core/Context.cpp:383-398
        EndFrame（EndRenderingPresent）→ SubmitFrameToRenderTarget（:389/:394）
```

### 4.2 pass 外的 CPU 阶段（`PrepareRenderPassSetup`）

`src/ecs/core/Context.cpp:719-732` 的注释即契约：「所有 CPU 与上传工作在 BeginRenderPass 之前，pass 内只发 GPU 绘制命令」。

```
SetFrameIndex(frameIndex)                  Context.cpp:726 → :1113（给 L2W ring 用）
RunRenderPhaseUpdates(RenderCollect)       Context.cpp:727（收集/剔除：PrimitiveCollect、Cull、Sort；Text/Line Collect）
RunRenderPhaseUpdates(RenderBatch)         Context.cpp:728（建批/写行表与命令表：PrimitiveBuild；Text/Line Build）
RenderBufferCommit(dt)                     Context.cpp:695-701（ViewUBOCommitSystem：相机/视口/天空 UBO）
RenderBufferUpload(dt)                     Context.cpp:703-709（同步 4-ID 存储 + 遍历脏 buffer 做 CopyToDevice + barrier）
RenderFrameSync(dt)                        Context.cpp:711-717（RenderSceneUBOSystem：挂 Scene 集）
```

### 4.3 pass 内：一遍 RenderGraph 循环

`ExecuteRenderGraphPasses`（`src/ecs/core/RenderGraph.cpp:91-160`）对每个 pass：

```
跳过 !pass.enabled 的 pass（:102-106）
pass.renderTarget 与当前 RT 不同 → 告警但按当前 RT 渲染（:112-118）
onBeforePass（可选）                        （:120-124）
runUpdate → RunRenderPhaseUpdates(max(start, RenderDrawSubmit), endPhase)   （:126-138，只跑 pass 内相位）
runRender → RecordPreparedRenderPhaseRange(start, endPhase, dt, submitTransforms, "[ECS RENDER] Render")  （:140-144）
   （runRender == false 但 submitTransforms == true 时只调 TransformSystem::SubmitTransformUpdates()，:145-150）
onAfterPass（可选）                         （:152-156）
```

`RecordPreparedRenderPhaseRange`（`src/ecs/core/Context.cpp:400-421`）先按需提交变换，再 `RunRenderSystemsInRange`。

### 4.4 三个遍历函数的区别（容易踩）

| 函数 | 调用哪个虚函数 | 用途 |
|---|---|---|
| `RunRenderPhaseUpdates(phase)`（`src/ecs/core/Context.cpp:789-804`）/ `(min,max)`（:805-823） | `system->Update()` | 渲染阶段的 **CPU 工作**（收集、建批、上传、同步） |
| `RunRenderSystemsInRange(min,max)`（`src/ecs/core/Context.cpp:839-868`） | `system->Render(cmd, dt)` | pass 内 **录命令** |
| `RunSystemUpdate`（`src/ecs/core/Context.cpp:824-838`） | `system->Update()` 单系统 | 上两者的共用启用判断包装（tick 序也用它） |

三者都先 `Sort*Systems()`、都检查 `IsEnabled()`。**当前版本没有 `SystemProfiler` 计时，也没有 `[ECS] Update/Render Begin|End` 日志**（均 0 命中或已注释）——可观测性收敛到组清单日志与各系统自己的日志（§10）。

### 4.5 离屏 / 子 pass：`RenderTo(RenderPassRequest)`

`ECSContext::RenderTo(const RenderPassRequest &req)`（`inc/hgl/ecs/core/Context.h:246`，实现 `src/ecs/core/Context.cpp:445-547`）与便捷重载 `RenderTo(rt, clear, dt)`（:549-557）是**离屏/子 pass 的标准入口**：

```
保存 render_target → 对旧 RT WaitFence（共享 Camera UBO / L2W ring 上沿保护）
按需覆写目标 RT 的 clear 色（req.use_target_clear == false 时）
同步 RenderTargetSystem::SetRenderTarget(rt)（不同步会把主管线画进 depth-only 离屏 pass）
pass 级相机覆盖：req.camera 非空 → CameraSystem::SetOverrideCamera + 显式 Update(dt)
组装 RenderPassOptions（load_depth / use_scissor / scissor / clear_scissor_depth / clear_depth_value=0.0f）
BeginManagedRenderFrame(dt, /*need_swapchain_acquire=*/false, options)   ← 离屏 RT 无 swapchain 图
  → RenderDrawOnly(render_core->GetRenderCmd(), dt)                      ← 只录 RenderCollect..RenderStat
  → EndManagedRenderFrame(dt)
提交后对目标 RT WaitFence（下沿保护，再恢复主相机共享数据）
恢复 render_target / clear 色 / RenderTargetSystem 的 RT / 覆盖相机
```

`RenderPassRequest` 字段（`inc/hgl/ecs/core/RenderPassRequest.h:31-64`）：`target` / `clear` / `use_target_clear` / `delta_time` / `camera` / `load_depth` / `use_scissor` + `scissor` / `clear_scissor_depth` / `mobility_filter`。
`RenderPassOptions`（`inc/hgl/vk/VKCommandBuffer.h:116-125`）：`load_color` / `load_depth` / `depth_old_layout` / `use_scissor` + `scissor` / `clear_scissor_depth` / `clear_depth_value`（Reversed-Z 默认 0.0f）。
实际调用者：`example/Basic/ShadowMap.cpp:1097`、`example/Basic/CascadeShadowMap.cpp:683,704`（`RenderTo(const RenderPassRequest&)`）；`src/SceneGraph/module/OffscreenWorld.cpp:149` 走的是三参重载 `RenderTo(graph::IRenderTarget*, Color4f, float)`（`src/ecs/core/Context.cpp:549`）。

`RenderDrawOnly`（`src/ecs/core/Context.cpp:425-443`）本身不含相位逻辑，它只是 `RecordPreparedRenderPhaseRange(RenderCollect, RenderStat, dt, true, ...)`。

---

## 5. 组件层详解

| 组件 | 职责 | 关键数据 | 更新路径 |
|---|---|---|---|
| `TransformComponent` `inc/hgl/ecs/components/TransformComponent.h:23-195` | 空间变换（SOA 存储 + 层级） | `parent_id`/`child_ids`、`mobility`、`matrixDirty` + 缓存世界矩阵、fixed-pixel 参数、`TransformDataStorage::HandleID GetStorageHandle()`（:180） | Setter → `MarkDirty(mask)`；`TransformSystem` 消费 dirty；层级 API :145-154（`SetParent/AddChild/RemoveChild`）；Mobility :159-163（`IsMovable/IsStatic`） |
| `MaterialComponent` `inc/hgl/ecs/components/MaterialComponent.h:14-82` | 材质运行期状态（程序 + 两类行） | `data_index_row`（:23）、`material_row_cpu/gpu`（:26-27）、`material_texture_configuration` + `material_texture_row_cpu/gpu` + `material_texture_zero_row_gpu` + `material_texture_configuration_hash`（:31-35）、`program_dirty`/`runtime_dirty`/`valid`（:41-43）、`last_materialize_epoch`（:65） | `RenderPrimitiveCollectSystem` 解析/物化时写；`MarkValid/MarkProgramResolved/MarkResourcesPending/MarkFailed` 状态机 |
| `PrimitiveComponent` `inc/hgl/ecs/components/PrimitiveComponent.h:50-274` | 「画什么」+ 作者侧资源 + 4-ID 槽位 | `runtime_data_buffer/runtime_draw_range`（:109-110）、`namedMaterialTextureResources`（:117）、`materialDataResource`（:118）、`material_authored_generation`（:123）、`resolvedRuntimePipelineMap`（:130，按 RenderPass 键控）、`render_item_handle` + `render_item_descriptor` | 设置类 API 一律 `++material_authored_generation` + `InvalidateResolvedRuntimePipeline()`；`EnsureRuntimeGeometryBinding` 在首次解析时建运行时几何绑定 |
| `VisibilityComponent` `inc/hgl/ecs/components/VisibilityComponent.h:15-40` | 可见性开关，**直写共享 storage** | `visible` + `VisibilityDataStorage*` | `SetVisible` 直接更新 storage（`VisibilityDataStorage` `inc/hgl/ecs/support/VisibilityDataStorage.h:19-53`，支持祖先链查询 `IsInvisible`） |
| `BoundingBoxComponent` `inc/hgl/ecs/components/BoundingBoxComponent.h:26-237` | 剔除用的 AABB（SOA） | `storageHandle`（静态共享 `BoundingBoxDataStorage`）、world AABB + valid | `SetAABB` → `TouchChange`；`LineBoundsUpdateSystem` 写线的世界 AABB（**没有 `BoundingBoxUpdateSystem`，0 命中**） |
| `CameraComponent` `inc/hgl/ecs/components/CameraComponent.h:32-85` | 相机参数与控制模式 | `control_mode`(ViewModel 等)、`target/distance/yaw/pitch/min_distance/max_distance`、`local_camera_data/local_camera_info` 与裸指针 `camera_data/camera_info/viewport_info`、**`camera_id`（全局 CameraInfo 池行号，:81）**、`is_main_camera`（:84）、`matrix_dirty`（:85） | `CameraSystem` 每帧按模式计算 → `UpdateMatrices` |
| `RenderableComponent` `inc/hgl/ecs/components/RenderableComponent.h:18-45` | 可渲染基类（`PrimitiveComponent` 的查询基） | `visible`、`boundingRadius` | 查询基：`GetComponents<RenderableComponent>()` 召回派生 |
| `InstancedPrimitiveComponent` `inc/hgl/ecs/components/InstancedPrimitiveComponent.h` | 实例化图元（多 4-ID 一次写） | 实例容量 + 多描符读写 | RPCS 主循环走 `SetAllInstances4ID(..., false)` 分支（`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:1410-1423`） |
| `LinesComponent` / `TextComponent` | Line / Text 元素数据 | 局部包围盒等 | 各自 Collect/Build 系统 |

组件变更机制（`inc/hgl/ecs/core/Component.h:23-79`）：`version` 单调自增（:26，`GetVersion()` :50，`TouchChange` 时 `++version` :73）+ `change_mask` 位掩码（`TouchChange(mask)` / `ClearAllChanges()`，`TransformChange{Position,Rotation,Scale,Parent,WorldMatrix,Mobility}`）。`TransformComponent` 另用 `matrixDirty` + mask + `version` 判定是否需要重算（`TransformSystem::ShouldUpdateTransform`，`src/ecs/systems/tick/TransformSystem.cpp:457-476`）。

---

## 6. Tick 系统层详解

| 系统 | phase | 依赖声明 | 每帧做什么 |
|---|---|---|---|
| `InputSystem` `src/ecs/systems/tick/InputSystem.cpp:24` | `TickInput` | — | 采集输入；`EndFrame()` 由 `Context::Tick` 收尾（`src/ecs/core/Context.cpp:310-313`） |
| `TransformSystem` `src/ecs/systems/tick/TransformSystem.cpp:14` | `TickTransform` | — | `Update`（:29-101）：只遍历 **movable**，按 mask/version 决定 `UpdateIfDirty`；`UpdateStaticDirty`（:102-136）；`SubmitTransformUpdates`（:137-355）：static 脏则整批重算 → `EnsureTransformBuffer`（:356）→ `RefreshHandleOrder`（:414）→ `EnsureCapacity` → 静态段/移动段 ring 分开写 → `MarkDirtyRanges` |
| `CameraSystem` `src/ecs/systems/tick/CameraSystem.cpp:224` | `TickCamera` | **无显式依赖**（构造里 `// Declare dependencies` 后为空，:226-227；顺序由相位保证） | `CollectCameras`（:397）→ `CollectInput`（:407）→ `ProcessInput`（:463）→ `UpdateBasis`（:473）/`UpdateTransform`（:485）→ `UpdateMatrices`（:525，写 `camera_data`+`camera_info`，清 `matrix_dirty`）；`CommitCameraUBO`（:310-319）**无条件全量写** view 三件套；`BindCameraResources`（:631）维护 `camera_id` 与 CameraInfo 行池行 |
| `VisibilitySystem` `src/ecs/systems/tick/VisibilitySystem.cpp:13` | `TickTransform` | — | 构造 `VisibilityDataStorage`；`Initialize`（:23-51）时把 storage 塞给所有已存在的 `VisibilityComponent` 并同步初值；`Update`（:52）空转（组件直写） |
| `LineBoundsUpdateSystem` `src/ecs/systems/tick/LineBoundsUpdateSystem.cpp:13` | `TickTransform` | `AddDependency<TransformSystem>()`（:14） | 线的局部包围盒 → 建/更新 `BoundingBoxComponent`（局部 + 世界 AABB） |

`TransformSystem` 的两段结构是本层最容易被误读的地方：**Tick 阶段只处理 movable 的 CPU 侧 dirty**，static 与全部 GPU 侧（静态段重写 + movable ring 段填充）都在 `SubmitTransformUpdates()` 里，而它由渲染图在 pass 执行前调用（`src/ecs/core/Context.cpp:406-410`，`src/ecs/core/RenderGraph.cpp:148-149`）。SimpleSphere 的球是 `Mobility::Movable`，所以每帧都进 ring 段。

---

## 7. 渲染系统层详解（以 Primitive 组为主线）

### 7.1 相位 × 系统矩阵

| phase | Primitive 组 | Text 组 | Line 组 | 其它渲染系统 |
|---|---|---|---|---|
| `RenderPreBeginFrame` | — | — | — | `EnvironmentSystem`（`src/ecs/systems/render/EnvironmentSystem.cpp:14`）、`ColorPaletteSystem`（`src/ecs/systems/render/ColorPaletteSystem.cpp:17`）、`RenderTargetSystem`（`src/ecs/systems/render/RenderTargetSystem.cpp:12`） |
| `RenderSwapchainNextImage` | — | — | — | `SwapchainNextImageSystem`（`src/ecs/systems/render/SwapchainNextImageSystem.cpp:11`） |
| `RenderCollect` | `RenderPrimitiveCollectSystem`（`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:383`）+ `PrimitiveCullSystem`（`src/ecs/support/primitive/PrimitiveCullSystem.cpp:10`）+ `PrimitiveSortSystem`（`src/ecs/support/primitive/PrimitiveSortSystem.cpp:10`） | `TextCollectSystem`（`src/ecs/support/text/TextCollectSystem.cpp:10`） | `LineCollectSystem`（`src/ecs/support/line/LineCollectSystem.cpp:11`） | — |
| `RenderBatch` | `PrimitiveBuildSystem`（`src/ecs/support/primitive/PrimitiveBuildSystem.cpp:10`） | `TextBuildSystem`（`src/ecs/support/text/TextBuildSystem.cpp:10`）、`TextSyncSystem`（`src/ecs/support/text/TextSyncSystem.cpp:10`） | `LineBuildSystem`（`src/ecs/support/line/LineBuildSystem.cpp:12`） | — |
| `RenderBufferCommit` | — | — | — | `ViewUBOCommitSystem`（`src/ecs/systems/render/ViewUBOCommitSystem.cpp:14`） |
| `RenderBufferUpload` | — | — | — | `RenderBufferUploadSystem`（`src/ecs/systems/render/RenderBufferUploadSystem.cpp:17`） |
| `RenderFrameSync` | — | — | — | `RenderSceneUBOSystem`（`src/ecs/systems/render/RenderSceneUBOSystem.cpp:99`，即原 `RenderDescriptorBindingSystem`） |
| `RenderDrawSubmit` | `PrimitiveRenderSystem`（`src/ecs/support/primitive/PrimitiveRenderSystem.cpp:30`） | `TextRenderSystem`（`src/ecs/support/text/TextRenderSystem.cpp:12`） | `LineRenderSystem`（`src/ecs/support/line/LineRenderSystem.cpp:13`） | — |
| `RenderDebug` | `PrimitiveOverlayRenderSystem`（`src/ecs/support/primitive/PrimitiveOverlayRenderSystem.cpp:26`） | — | — | — |
| `RenderStat` | — | — | `LineStatsSystem`（`src/ecs/systems/render/LineStatsSystem.cpp:12`） | — |
| `RenderSubmit` | — | — | — | `SwapchainSubmitSystem`（`src/ecs/systems/render/SwapchainSubmitSystem.cpp:14`） |

**`RenderFrameUBOSyncSystem` 不存在**（0 命中）：`RenderFrameSync` 相位上只有 `RenderSceneUBOSystem`，它的职责是场景 UBO 数据流 + 材质化资源注册（`inc/hgl/ecs/systems/render/RenderSceneUBOSystem.h:35-43` 类注释）。

显式依赖只有 4 条（§3.2）；其余顺序全部由相位决定。

### 7.2 薄代理系统 + pipeline 的双层结构

`RenderPipelineSystem` 家族把「相位 × 元素类型」标准化（`inc/hgl/ecs/support/RenderPipelineSystem.h:24-153`）：

```
RenderPipelineSystem::ValidatePipeline(context)     src/ecs/support/RenderPipelineSystem.cpp:6-22
   = IsEnabled() && context && GetPipeline(context) != nullptr
CollectSystem::Update       override final → OnCollect(pipeline)     :24-33
CullSystem::Update          override final → OnCull                  :36-45
SortSystem::Update          override final → OnSort                  :48-57
BuildSystem::Update         override final → OnBuild                 :60-69
SyncSystem::Update          override final → OnSync                  :72-81
RenderPipelineDrawSystem::Render override final → OnRender(pipeline, cmd)  :84-91
```

于是具体系统只剩三步样板（`src/ecs/support/primitive/PrimitiveCullSystem.cpp:7-22` 为例）：构造里 `SetExecutionPhase(...)` + `SetRenderElementType("Primitive")`；`GetPipeline()` 返回 `context->GetRenderPipeline("Primitive")`；`OnCull` 转调 `pipeline->RunCull()`。**Update/Render 被 `final` 封死**——想插入逻辑必须经 pipeline 或新加阶段基类。

pipeline 侧接口 `RenderPipelineBase`（`inc/hgl/ecs/support/RenderPipelineBase.h:16-56`）：`GetName/GetWorld/PrepareFrame/RunCollect/RunCull/RunSort/RunBuild/RunSync/Render/Shutdown`。`PrimitiveRenderPipeline` 只是把请求转给 `PrimitiveBatchPipeline` 实现体（`src/ecs/support/primitive/PrimitiveRenderPipeline.cpp:24-56`）：

```
PrepareFrame → impl_->PrepareFrame(context_)
RunCull      → PrepareFrame() + RunCulling()
RunSort      → PrepareFrame() + RunSorting()
RunBuild     → PrepareFrame() + RunTransformIndexing() + RunBatching()
Render       → 空实现（绘制由 PrimitiveRenderSystem 直读帧缓存发给 PipelineMaterialRenderer）
```

`PrimitiveBatchPipeline::PrepareFrame`（`src/ecs/support/PrimitiveBatchPipeline.cpp:89-118`）是幂等守门：确认 `cache.renderItems` 非空、取 cameraInfo/device、对齐帧序号——所以 Cull/Sort/Build 三个系统各自调都没问题。`PrimitiveBatchPipeline` 的成员声明见 `inc/hgl/ecs/support/PrimitiveBatchPipeline.h:31-76`。

### 7.3 收集阶段：RenderPrimitiveCollectSystem

`Update`（`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:1090-1449`）是本层最重的系统：

1. **帧缓存开帧**：`cache.BeginFrame()` 清 `renderItems`、清计数、调各 `MaterialBatch::Clear()` 但保留 batch 对象复用（:1106-1108）；
2. **取可见性 storage**（O(1) 跳过不可见实体，:1110-1116）+ `active_mobility_filter`（:1118）；
3. **预扫描 + 物化 epoch**（:1141-1216）：逐 primitive 判 `fast_path_holds`（program 未脏 + `tracked_material_authored_generation` 一致 + recipe hash 非 0，:1186-1191）、`epoch_stale`（用到 runtime rows 且 `last_materialize_epoch` 落后，:1193-1195），合并成 `needs_work`（:1207-1209）；`any_material_work && any_possible_runtime_rows_visible` 时 `++materialize_epoch`（:1215-1216；epoch 语义：物化会重写共享行表，被跳过的 primitive 下次必须重物化）；
4. **主循环**（:1225-1439）：过滤 `!IsVisible / !CanRender` / storage 不可见 / 无 owner / 无 transform / mobility filter / 无 recipe → 拿或建 `MaterialComponent` → 三条路径：
   - **全清帧快速路径**（`fast_path_holds && last_materialize_epoch == materialize_epoch`，:1286-1311）：只跑 `ResolveMaterialProgramForPrimitive` + `EnsureRuntimeGeometryFromAsset` + `ResolveRuntimePipelineForPrimitive`，成功 `MarkValid()`；
   - **需要工作帧**（:1312-1372）：`MarkResourcesPending()` → `PrepareActivePlanResources` → `MaterializeRecipeRowsForPrimitive` → `EnsureRuntimeGeometryFromAsset` → `ResolveRuntimePipelineForPrimitive` → `MarkValid()`；任一步失败 `InvalidateRecipeRuntime` + `MarkFailed()`（`valid` 保持 false，下一帧重试而不是静默跳过）；
5. **产出 4-ID 并生成 RenderItem**（:1377-1438）：`transform_id = transform->GetStorageHandle()`、`geometry_id`（必要时现场 `EnsureMeshDrawParams`，:1394）、`material_id = data_index_row`、`texture_id = material_texture_configuration.row_index`；`PrimitiveComponent::Set4ID(...)` / `SetAllInstances4ID(...)`（`inc/hgl/ecs/components/PrimitiveComponent.h:257`、`src/ecs/components/PrimitiveComponent.cpp:672-683`）；`make_unique<PrimitiveRenderItem>`（或 `InstancedPrimitiveRenderItem`），填 `distanceToCamera = length(worldPos - camera_pos)`（:1432），`UpdateWorldMatrix()`（:1434），push 进 `cache.renderItems`，`renderableCount++`（:1437）。

材质行地址物化在 `MaterializeRecipeRowsForPrimitive`（:653-1088）里：`TryGetRowBuffer`（:750）→ `IsActive(type, data_index)`（:762）→ 容量检查（:779-791）→ `material_row_gpu = gpu_base + data_index*row_bytes`（:801）、`data_index_row = data_index`（:815）；纹理引用行走 `AcquireMaterialTextureConfiguration`（:990）/ `WriteMaterialTextureConfiguration`（:1004）/ `RetireMaterialTextureConfiguration`（:1010）。

`PrimitiveRenderItem`（`inc/hgl/ecs/core/PrimitiveRenderItem.h:28-72`、`src/ecs/core/PrimitiveRenderItem.cpp`）是 `RenderItem` 接口的实现：对外暴露实体/组件句柄、`GetShaderProgram/GetPipeline(render_pass)/GetGeometryDataBuffer/GetGeometryDrawRange/GetRenderItemHandle`（后两者转问 `PrimitiveComponent` 的运行时绑定），构造时缓存世界矩阵。批处理只需要 `RenderItem*` 这层接口（`inc/hgl/ecs/core/RenderItem.h:37-72`，含 `index` / `transform_index` / `distanceToCamera` / `isVisible` 与 `Compare`）——这是 ECS 与渲染层解耦的接缝。

### 7.4 批处理阶段：PrimitiveBatchPipeline

| 步骤 | 函数（`src/ecs/support/PrimitiveBatchPipeline.cpp`） | 内容 |
|---|---|---|
| 剔除 | `RunCulling → PerformFrustumCulling` `:120/:153-186` | 视锥测试（`TestFrustumWithWorldAABB` `:187` / `LocalAABB` `:197` / `BoundingSphere` `:209`），置 `item->isVisible` |
| 排序 | `RunSorting → SortByDistance` `:125/:231-243` | 按 `distanceToCamera` |
| 变换索引 | `RunTransformIndexing → AssignTransformIndices` `:130/:244-302` | 由 handle 查组内序号，movable → `dynamic_base + group_index`，static → `group_index + 1`（slot 0 恒为单位矩阵）；写 `item->transform_index` |
| 成批 | `RunBatching → BuildMaterialBatches` `:147/:1014-1088` | 按 `ShaderProgramPipelineKey{shader_program, pipeline}`（`inc/hgl/ecs/core/ShaderProgramPipelineKey.h:23-48`）归批；缺 recipe rows 或未解析 pipeline 的 item 直接跳过并告警 |
| 批内定型 | `FinalizeBatches → FinalizeBatch` `:1090-1102 / :732-784` | `SortBatchItems`（:786-831）→ `EnsureBatchIndexRows`（:833-916）→ `EnsureMeshDrawParams`（:591-642）→ `BuildBatches`（:366-589，生成 DrawBatch，同几何同 range 合并实例）→ `WriteBatchIndexRows`（:918-1013）+ `WriteMeshDrawCommands`（:644-730） |

`MaterialBatch`（`inc/hgl/ecs/core/MaterialBatch.h:40-94`）是「一个 shader program 的全部待绘项」：`key`（shader+pipeline）、`items`（`RenderItem*`）、`draw_batches`/`draw_batches_count`、**`mesh_draw_params_buffer`（现装 `MeshDrawCommand` 表，每 DrawBatch 一行 8B）+ `mesh_draw_params_capacity`**、`icb_mesh_tasks`（mesh 间接命令）、`icb_count_buffer`/`icb_count_buffer_offset`（GPU 计数，可选）、`l2w_index_buffer`、`material_data_index_rows_buffer`、`texture_reference_base_addr`、`gpu_driven_override`/`uses_render_item_resolve`/`own_*` 所有权位、`transform_buffer`（非拥有，来自 TransformSystem）、`renderer`（`PipelineMaterialRenderer`）。

### 7.5 提交阶段：从帧缓存到 vkCmd

`PrimitiveRenderSystem::OnRender`（`src/ecs/support/primitive/PrimitiveRenderSystem.cpp:39-94`）：从 `cache.materialBatches` 收集非空批 → 按 `MaterialBatch::key` 排序（:60-65）→ 跳过 `pipeline->GetOverlay()` 的批（:72-73，交给 Overlay 系统）→ 逐个 `renderer->Render(cmdBuffer, draw_batches, count, transform_buffer, batch, context->GetRenderContext(), context->GetActiveCameraID())`（:86-92）。批次内动作见配套文档 `doc/simple-sphere-ecs-render-chain.md` §5。

### 7.6 描述符现状（BDA 终态）

| 项 | 现状 |
|---|---|
| 描述符集 | **只有 2 个**：Scene(0)（6 个 UBO：Camera/Sky/Viewport/ColorPalette/GlobalAddresses/Shadow）与 Bindless(1)（纹理数组 + sampler 数组 + cube 数组）。真源 `inc/hgl/common/DescriptorSetTypeDef.h:12-24`（`enum class SceneBinding`）/:52-64（`enum class DescriptorSetType`） |
| per-material / per-object / per-draw 描述符集 | **已退役**：`PerObject/Material/Vertex` 集随 BDA 化删除（`inc/hgl/common/DescriptorSetTypeDef.h:55-60` 注释明示）；`MaterialBind` 之类的绑定动作 0 命中 |
| 绑定点 | `GraphicsContext::BindGlobalDescriptorSets(cmd, layout)`（`inc/hgl/graph/core/GraphicsContext.h:140`，compute 版 :143），Scene 集数据由 `RenderSceneUBOSystem::ApplyResourceLayoutBindings`（`src/ecs/systems/render/RenderSceneUBOSystem.cpp:378-410`）写入，Bindless 由 `BindlessTextureManager` 维护 |
| 材质行 / 纹理引用行的载体 | `GlobalSSBOBufferRegistry`（`ActiveRowPool` 行池，`inc/hgl/graph/module/GlobalSSBOBufferRegistry.h:88`）+ `MaterialTextureReferencePool`（每 (definition, layout) 一池，`inc/hgl/graph/module/MaterialTextureReferencePool.h:49-128`） |
| 行寻址 | `payload_index`（→ 全局池行号）/ `texture_reference_index`（→ 纹理引用池行号），经 `pc_root.addr_mtl_data_addrs` 行表 + `pc_root.addr_texture_references` 基址 BDA 解引用（`src/ShaderGen/compile/MaterialShaderEmitter.cpp:291-325`） |
| 地址载体 | ① `GlobalAddressesInfo` UBO（Set0/binding4，7×uint64=56B，启动写一次，`inc/hgl/graph/ubo/GlobalAddresses.h:14-25`）② `RootAddresses` push constant（72B，每 MaterialBatch 一次，`inc/hgl/graph/RootAddressPush.h:28-64`） |
| 顶点路径 | mesh shader（`vkCmdDrawMeshTasksIndirectEXT`）是唯一路径；**无 `vkCmdBindVertexBuffers` / `vkCmdBindIndexBuffer` / vertex input state**（全仓 0 命中） |

---

## 8. ECS 与 GPU 的接口面（谁写哪块 GPU 数据）

| GPU 数据 | 写入者（ECS 侧） | 载体 |
|---|---|---|
| L2W 矩阵（静态段 + movable ring 段） | `TransformSystem::SubmitTransformUpdates`（tick 系统，渲染前被图调用） | `TransformAssignmentBuffer`（`src/ecs/support/TransformAssignmentBuffer.cpp`，`GetTransformDataBuffer()` `inc/hgl/ecs/support/TransformAssignmentBuffer.h:73`） |
| 一级 4-ID 描述符表 | `RenderItemDataStorage`（组件挂载时 `Allocate`、RPCS 每帧 `Set4ID`） | `RenderItemDataStorage`（`inc/hgl/ecs/support/RenderItemDataStorage.h:29-130`） |
| 二级绘制索引表 | `DrawItemIDStorage::Append`（批处理连号折叠 / GPU-Driven 路径） | `DrawItemIDStorage`（`inc/hgl/ecs/support/DrawItemIDStorage.h:25-90`） |
| L2W 索引行 / `MaterialInstanceAddresses` 行 | `PrimitiveBatchPipeline::WriteBatchIndexRows`（RenderBatch 相位） | `MaterialBatch::l2w_index_buffer` / `material_data_index_rows_buffer` |
| `MeshDrawCommand` 行（geometry_id + first_instance）+ mesh 间接命令 | `PrimitiveBatchPipeline::WriteMeshDrawCommands` | `MaterialBatch::mesh_draw_params_buffer` / `icb_mesh_tasks` |
| 几何参数行（112B `MeshDrawParams`，含全部顶点流 BDA 地址） | **几何创建期**：`Geometry::RegisterMeshDrawParams`（由 `GeometryCreater::Create()` 自动调用） | `GlobalSSBOBufferRegistry` 的 `MeshDrawParams` 池，按 `geometry_id` 索引 |
| 材质参数行（`PBRSurfaceRow` …） | 作者侧 `GlobalSSBODataAccessor::Write`；地址由 RPCS `MaterializeRecipeRowsForPrimitive` 解析 | `GlobalSSBOBufferRegistry` 的 `ActiveRowPool` |
| 纹理引用行（`uvec2` handle/layer） | RPCS 材质化（`AcquireMaterialTextureConfiguration` / `WriteMaterialTextureConfiguration`） | `MaterialTextureReferencePool`（按 definition 一池） |
| CameraInfo / SkyInfo / ViewportInfo UBO | `CameraSystem::CommitCameraUBO`（RenderBufferCommit）+ `ViewUBOCommitSystem`；挂载由 `RenderSceneUBOSystem::ApplyResourceLayoutBindings` | Scene 描述符集（set 0）；**相机在 GLSL 侧经 `CameraInfoBufferRef` + `pc_root.camera_id` 从池读**（`ShaderLibrary/ubo/scene_ubo.glsl:123`） |
| 脏 buffer 的 GPU 传输 + barrier | `RenderBufferUploadSystem::Update`（先同步两个 4-ID storage，再遍历 `device->GetGPUBufferRegistry()`） | `IGPUBuffer::CopyToDevice` + `MemoryBarrier2`（`src/ecs/systems/render/RenderBufferUploadSystem.cpp:46-56,135-148`） |
| swapchain 图获取 / 提交 | `SwapchainNextImageSystem`（帧首）/ `SwapchainSubmitSystem`（帧尾） | `SwapchainRenderTarget::NextFrame/Submit` |

---

## 9. 帧缓存与生命周期

- `RenderFrameCache`（`inc/hgl/ecs/core/Context.h:48-63`）：`renderItems`（`unique_ptr` 数组，每帧重建）、`materialBatches`（key→`MaterialBatch` 的 map，**跨帧保留对象**只 `Clear()` 内容）、`cameraInfo`、`renderableCount`；实例挂载在 `ECSContext::render_frame_cache`（`inc/hgl/ecs/core/Context.h:128`），经 `GetRenderFrameCache()` 暴露。
- 跨帧存活的还有：`TransformAssignmentBuffer`（TransformSystem 持有）、`MaterialBatch` 的各类 buffer（`Ensure*` 按 pow2 扩容）、行池（`GlobalSSBOBufferRegistry` / `MaterialTextureReferencePool` 持有）、`RenderItemDataStorage` / `DrawItemIDStorage`（`ECSContext` 成员，`inc/hgl/ecs/core/Context.h:122-123`，构造时创建于 `src/ecs/core/Context.cpp:64-65`）。
- 非拥有关系要小心：`MaterialBatch::transform_buffer` 是 TransformSystem 的指针（`inc/hgl/ecs/core/MaterialBatch.h:78` 注释明示勿跨帧缓存系统指针）；`PrimitiveRenderItem` 持 `shared_ptr` 组件句柄（会延长组件寿命到当帧结束）。
- `frame_index`（`SetFrameIndex`，`src/ecs/core/Context.cpp:1113-1124`）供 ring 分段；`render_submission_serial`（`inc/hgl/ecs/core/Context.h:130`）供资源回收（纹理引用池的 retire 延迟与此挂钩，常量 `MaterialTextureConfigurationRetireEpochDelay = 3u`，`inc/hgl/graph/module/MaterialTextureReferencePool.h:15`）。
- 活跃相机/可见性过滤：`active_camera_id`（`inc/hgl/ecs/core/Context.h:148`，`GetActiveCameraID()` :298）与 `active_mobility_filter`（:151，`GetActiveMobilityFilter()` :266）由 `RenderTo` 的 pass 级设置在区间内覆盖，供 RPCS 与渲染器消费。

---

## 10. 可观测性

| 手段 | 位置 / 形态 |
|---|---|
| 组注册清单 | `[RenderGraph] Registered %zu system groups (total %zu)`（`src/ecs/core/RenderGraph.cpp:83-84`）+ `SystemGroupRegistry::DebugPrint()`（同处 :85，定义 `inc/hgl/ecs/core/SystemGroup.h:88`）；`[RenderGraph]   active group: %s` 逐个活跃组（`src/ecs/core/RenderGraph.cpp:211`） |
| 帧边界 / pass 执行日志 | **当前已注释**（`src/ecs/core/RenderGraph.cpp:168,176` 的 Frame Start/End；:104/:108 的 skip/execute pass；`src/ecs/core/Context.cpp:412-418` 的 phase range）。需要时打开对应注释行 |
| 系统级计时 | **`SystemProfiler` 已删除**（0 命中）。逐系统 `[ECS] Update/Render Begin|End` 日志也已注释（`src/ecs/core/Context.cpp:824-868` 的 `RunSystemUpdate` / `RunRenderSystemsInRange`）——「某系统没跑」改由组开关 + 系统自身日志判断 |
| 材质/寻址诊断 | `ULRE_ARENA_DEBUG` 环境变量开启 `[ArenaTrace]`（`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:684/:735/:806`）；`[ArenaDebug]` 首次行表诊断由 `MaterialBatch::debug_blocks_logged` 控制（`inc/hgl/ecs/core/MaterialBatch.h:80`） |
| 渲染层一次性日志 | `[IndirectMeshDraw] mesh indirect flush engaged: first=%d count=%u`（`src/ecs/support/PipelineMaterialRenderer.cpp:47-48`）；`[MaterialTextureReferencePool] created …`（`src/SceneGraph/module/MaterialTextureReferencePool.cpp:82-89`）；`[LineStats] total=…`（`src/ecs/systems/render/LineStatsSystem.cpp:34`）；`[SceneUBO] Scene UBO set not bound: …`（`src/ecs/systems/render/RenderSceneUBOSystem.cpp:404`） |

---

## 11. SimpleSphere 的 ECS 映射

| 示例代码（`example/Basic/SimpleSphere.cpp`） | ECS 对象 | 驱动者 | 产出 |
|---|---|---|---|
| `CreateEntity<Entity>("SphereEntity")`（:171） | Entity + EntityID | `Context::CreateEntity`（`inc/hgl/ecs/core/Context.h:369`） | EntityManager 表项 |
| `AddComponent<TransformComponent>(Mobility::Movable)`（:173）+ `SetMovable(true)`（:177） | TransformComponent | `RegisterComponentInstance`（建 TransformComponent 时另走 `RegisterTransformComponent` 进 movable 列表） | movable_transforms 一项 |
| `SetLocalPosition/Rotation` + 每帧 `Tick` 自转（:248-258） | 同上（mask 变化） | `TransformSystem::Update` → `SubmitTransformUpdates` | L2W 矩阵写入 movable ring 段 |
| `AddComponent<PrimitiveComponent>()`（:180）+ `SetPrimitiveAsset/SetMaterialTextureResource/SetMaterialDataResource`（:183-191） | PrimitiveComponent（`material_authored_generation++`） | 反向注册 → 自动安装并打开 **"Primitive" 组** | 组激活 + 系统注册 |
| `sphere_recipe`（Lit/PBR，:115-119） | 挂在 `PrimitiveAsset` 上 | `RenderPrimitiveCollectSystem` 解析/物化 | `MaterialComponent.program` + `data_index_row` + `material_row_gpu` + `material_texture_row_gpu` |
| `ecs_context->EnsureCameraSystem()`（:199）+ `CreateEntity<Entity>("MainCamera")`（:202）+ `AddComponent<CameraComponent>`（:203，`ControlMode::ViewModel`、`is_main_camera=true`、`matrix_dirty=true`） | CameraComponent | `CameraSystem` | `camera_id` → 全局 CameraInfo 行池 → Scene 集可读 |
| 球体几何（私有 VAB/IBO，`GeometryCreater::Create` @`src/SceneGraph/geo/GeometryCreater.cpp:194`） | `PrimitiveComponent::runtime_data_buffer`（GeometryDataBuffer） | RPCS `EnsureRuntimeGeometryFromAsset` | `geometry_id`（全局 `MeshDrawParams` 行号）+ 行为 11 个 `addr_*` |

单实体场景下 ECS 的实际执行面：**1 个 Entity、3 个组件**（Transform/Primitive/Material，MaterialComponent 由收集系统自动补建）、**1 个 MaterialBatch、1 个 DrawBatch、1 行 `MeshDrawCommand`、1 行材质地址、1 行纹理引用**——渲染层表现为 `[IndirectMeshDraw] mesh indirect flush engaged: first=0 count=1`。

---

## 12. 扩展点与雷区

1. **不要手写遍历顺序**：顺序 = `ExecutionPhase` + 显式依赖（`AddDependency<T>`，`inc/hgl/ecs/core/System.h:110-115`）+ 注册先后；想插到某系统前就加依赖，别改相位数值。
2. **Update 与 Render 是两套遍历**（§4.4）：`RenderCollect/RenderBatch/...` 阶段的工作写在 `Update()` 里，pass 内录命令写在 `Render()` 里；把录命令逻辑写进 `Update()` 会在 cmd buffer 未开时崩。
3. **`final` 封死的阶段基类**：要扩展 Collect/Build/Render 的行为，必须经对应 pipeline（`RenderPipelineBase` 虚函数）或新增阶段基类，不能直接 override。
4. **组开关是单向收敛的**：`CreateAdaptiveRenderGraph` 每帧按 `SceneStats` 推导组开关，组件计数归零即 `SetElementTypeSystemsEnabled(false)` 关掉整组系统；因此「系统没跑」的第一嫌疑是**组里没有组件**（而不是系统没注册）。新增元素类型必须同时提供：组件（`GetSystemGroupName`）+ 组安装器（`DefaultSystems`）+ 相位区间可推导的系统。
5. **图构造依赖组元数据**：`EnsureSystemGroupsRegistered` 必须先于建图（`src/ecs/core/DefaultSystems.cpp:240` 已保证）；运行期新出现的元素类型会让 `MarkSceneStructureDirty` 生效，下一帧重建图时才真正换 pass 集合。
6. **帧缓存生命周期**：`cache.renderItems` 每帧重建（持有对象的是 `unique_ptr`），`materialBatches` 跨帧复用但每帧 `Clear()`——**不要跨帧缓存 `RenderItem*`**；`MaterialBatch::transform_buffer` 是非拥有指针。
7. **物化 epoch 语义**：共享行表被重写会让「本帧被跳过的 primitive」行号失效，因此 `materialize_epoch` 不匹配时必须重物化（RPCS `:1141-1216` 的预扫描就是为此）。改收集逻辑时保留这条不变量。
8. **静态/移动双通道**：`Mobility::Static` 的 GPU 写只在 `SubmitTransformUpdates` 里按需整批重写，`Movable` 每帧进 ring 段。把大量常驻对象设成 Movable 会每帧付出全量 ring 写代价。
9. **`geometry_id == 0` 是「未注册」而不是「第 0 行」**：几何参数行 0 是预留零行（全 0 地址），shader 解引用不崩但什么都不画。`GeometryCreater::Create()` 会自动注册，私有构造路径要自己 `EnsureMeshDrawParams`。
10. **诊断开关**：`ULRE_ARENA_DEBUG`（材质/寻址）、`SystemGroupRegistry::DebugPrint`（组定义）+ `[RenderGraph] active group`（谁在跑，需临时打开注释）、`[IndirectMeshDraw]`（合批是否生效）——排查「某系统没生效」按三层依次排除：组是否启用 → 系统是否注册/启用 → 相位是否被 pass 覆盖。

---

## 13. 基线后的新进展（以代码为准）

本节以现状为基线，逐条列本次校正涉及的「旧说法 → 现状 + 依据」。

### 13.1 执行阶段名与相位表

| 旧说法 | 现状 | 依据 |
|---|---|---|
| `RenderResourceSetup`（惰性一次性 GPU 资源） | 相位不存在 | `inc/hgl/ecs/core/System.h:22-48`；全仓 0 命中 |
| `RenderMaterialBind`（每实体材质/纹理绑定） | 相位不存在；材质/纹理经全局行池 + BDA，无 per-entity 绑定相位 | 同上；`src/ShaderGen/compile/MaterialShaderEmitter.cpp:291-325` |
| `RenderBeginFrame`（开 cmd、录帧 UBO） | 相位不存在。开 cmd buffer 是 `RenderSystemCore::BeginFrame()`（非 System），帧 UBO 写在 `RenderBufferCommit` | `src/ecs/systems/render/RenderSystemCore.cpp:42`；`src/ecs/systems/render/ViewUBOCommitSystem.cpp:17-36` |
| `RenderPostProcess` | 相位不存在；pass 内只有 `RenderDrawSubmit` 与 `RenderDebug` | `inc/hgl/ecs/core/System.h:42-44` |
| 用数字指代相位（9/10/11/12/13/14/16） | 数字标签废弃。真实渲染段为 `RenderSwapchainNextImage(4)` → `RenderPreBeginFrame(5)` → `RenderCollect(6)` → `RenderBatch(7)` → `RenderBufferCommit(8)` → `RenderBufferUpload(9)` → `RenderFrameSync(10)` → `RenderDrawSubmit(11)` → `RenderDebug(12)` → `RenderStat(13)` → `RenderSubmit(14)` | `inc/hgl/ecs/core/System.h:22-48` |
| 「pass 0 的各 render phase」 | pass 由组定义推导，数量 = 命中组数；每 pass 携带 `startPhase..endPhase` | `src/ecs/core/RenderGraph.cpp:229-253` |

### 13.2 系统名 / 系统组名

| 旧说法 | 现状 | 依据 |
|---|---|---|
| `RenderFrameUBOSyncSystem` | 类型不存在（0 命中）。`RenderFrameSync` 相位上只有 `RenderSceneUBOSystem` | `src/ecs/systems/render/RenderSceneUBOSystem.cpp:99`；`inc/hgl/ecs/systems/render/RenderSceneUBOSystem.h:35-43` |
| `RenderDescriptorBindingSystem` | 已改名 `RenderSceneUBOSystem`（类注释保留改名记录） | `inc/hgl/ecs/systems/render/RenderSceneUBOSystem.h:35` |
| `BoundingBoxUpdateSystem` | 类型不存在（0 命中）。世界 AABB 由 `LineBoundsUpdateSystem`（线）与几何注册流程维护；`TransformSystem` 管 L2W | `src/ecs/systems/tick/LineBoundsUpdateSystem.cpp:13` |
| `SystemProfiler`（per-system / per-group 计时） | 类型不存在（0 命中），`componentCount` 组状态字段也随之删除 | 全仓 0 命中 |
| `RenderSystemGroup` / `RenderSystemGroupRegistry` / `ScopedGroupState`（旧别名） | 全部删除（0 命中）；只有 `SystemGroup` / `SystemGroupRegistry` | `inc/hgl/ecs/core/SystemGroup.h:28-89` |
| `SystemGroup{name, startPhase, endPhase, enabled}` | **`enabled` 字段已移除**：组开关是每世界状态，由 `CreateAdaptiveRenderGraph` 按 `SceneStats` 即时推导，不再存全局注册表 | `inc/hgl/ecs/core/SystemGroup.h:28-43,45-52`；`src/ecs/core/RenderGraph.cpp:226-240` |
| `ScopedSystemGroupState` 临时开关 | 不存在（0 命中）；开关唯一入口 `SetElementTypeSystemsEnabled` | `src/ecs/core/Context.cpp:1342-1356` |
| `OnCreate` / `OnDependenciesReady`（System 生命周期） | 两个钩子都不存在（0 命中）。System 生命周期 = `Initialize / Update / Render / Shutdown`；`ECSContext::Initialize` 里直接按拓扑序调 `Initialize()` | `inc/hgl/ecs/core/System.h:75-84`；`src/ecs/core/Context.cpp:136-151` |
| `Context::CreateChildEntity(parent, desc)` | API 不存在（0 命中）。层级改为 `TransformComponent::SetParent/AddChild/RemoveChild` + `CreateEntity<Entity>` | `inc/hgl/ecs/components/TransformComponent.h:145-154` |
| `DisableUnusedSystemGroups` / `CleanupSystemGroup` | 两个 API 都不存在（0 命中）。组关闭只发生在 `CreateAdaptiveRenderGraph` 与反注册计数归零路径 | `src/ecs/core/RenderGraph.cpp:230-237`；`src/ecs/core/Context.cpp:1154-1187` |
| `SystemOwnershipScope{GlobalShared,LocalIsolated,Auto}` × `ContextRole{RootShared,LocalSubWorld}` 及被拒计数器 | **全部不存在**（0 命中）。注册无权限门，只有 `RegisterSystem/RegisterTickSystem/RegisterRenderSystem` 与 `Ensure*System` 幂等包装 | `inc/hgl/ecs/core/Context.h:422-443`；`src/ecs/core/DefaultSystems.cpp:39-62` |
| `RegisterTickSystemScoped` / `RegisterRenderSystemScoped` | 不存在（0 命中） | 同上 |
| `CameraSystem` 依赖 `InputSystem`、`TransformSystem` | 构造里依赖声明为空（仅一句注释）；顺序由相位 `TickInput < TickTransform < TickCamera` 保证。当前仓库显式依赖只有 4 条（LineBoundsUpdate→Transform、TextCollect→RenderPrimitiveCollect、TextSync→TextBuild、TextRender→PrimitiveRender） | `src/ecs/systems/tick/CameraSystem.cpp:219-232`；`grep AddDependency<` 命中 4 处 |
| `swapchain 提交系统依赖三个 RenderSystem` | `SwapchainSubmitSystem` 未声明显式依赖，靠相位 `RenderSubmit` 保证在三个 `RenderDrawSubmit` 系统之后 | `src/ecs/systems/render/SwapchainSubmitSystem.cpp:14`；`grep AddDependency<` 无该文件 |
| `RenderSystemGroup` 里「把每个已注册系统组变成一段启用 pass：`CreateDefaultLinearGraph`」 | 线性图路径删除，只有 `CreateAdaptiveRenderGraph`；`use_adaptive_render_graph` / `SetAdaptiveRenderGraphEnabled` 一并删除（0 命中） | `src/ecs/core/RenderGraph.cpp:217-262`；`src/ecs/core/Context.cpp:598-618` |

### 13.3 描述符 / 材质绑定 → 现载体

| 旧说法 | 现状 | 依据 |
|---|---|---|
| 「Scene UBO / Bindless 纹理两个集，首次绑一次」 | 表述正确，补充真源与写入者：`enum class SceneBinding`（`DescriptorSetTypeDef.h:12-24`）+ `DescriptorSetType`（:52-64）；Scene 集数据由 `RenderSceneUBOSystem::ApplyResourceLayoutBindings` 写，绑定由 `GraphicsContext::BindGlobalDescriptorSets` 做 | `inc/hgl/common/DescriptorSetTypeDef.h:12-64`；`src/ecs/systems/render/RenderSceneUBOSystem.cpp:378-410`；`inc/hgl/graph/core/GraphicsContext.h:140` |
| `MaterialSSBOBufferRegistry` 的 `ActiveRowPool` 写材质行 | 现载体 = `GlobalSSBOBufferRegistry`（`pools[GlobalSSBOTypeCount]`）+ `GlobalSSBODataAccessor`；容量来自 `kGlobalSSBOConfigs`（PBRSurface 1024×32B / MeshDrawParams 16384×112B / CameraInfo 64） | `inc/hgl/graph/module/GlobalSSBOBufferRegistry.h:31-61,88`；`src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp:13-20` |
| `MaterialTextureReferencePool`（按 definition 一池，uvec2 handle/layer） | 现为按 **(definition, layout)** 一池，行 0 预留零行，retire 走 `ReleaseDeferred`/`CollectRetired`，常量 `MaterialTextureConfigurationRetireEpochDelay = 3u` | `inc/hgl/graph/module/MaterialTextureReferencePool.h:15,40-47,102-110` |
| `MaterialSSBODataAccessor::Write` | `GlobalSSBODataAccessor::Write(row)` → `ActiveRowLease::Write` → `ActiveRowView::WriteAs` → `ActiveRowPool::CommitRow`（按行标脏） | `inc/hgl/vk/buffer/ActiveRowLease.h:157-166`；`inc/hgl/vk/buffer/ActiveRowView.h:174-188`；`src/Vulkan/buffer/ActiveRowPool.cpp:191-203` |
| `payload_address` / `texture_reference_address` | 字段改名 `payload_index` / `texture_reference_index`（行号语义） | `inc/hgl/graph/ShaderBufferSources.h:129-140`；`src/ecs/support/PrimitiveBatchPipeline.cpp:975-994` |
| `PBRSurfaceRowRef(...)`（凭地址直解引用） | 生成的宏形态为 `MTL_ROW(i) = PBRSurfaceRowRef(global_addresses.addr_pbr_surface + uint64(...values[i].payload_index) * 32)`；纹理侧 `MTL_TEX(i) = MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64(...values[i].texture_reference_index) * 48)` | `src/ShaderGen/compile/MaterialShaderEmitter.cpp:291-297,322-325` |
| `pc_root` 7 张表 / 56B | **72B / 8×uint64 + 2×uint32**：新增 `addr_texture_references`，`addr_mesh_draw_params` 语义变为「本批 MeshDrawCommand 表」，文本三表齐备 | `inc/hgl/graph/ShaderBufferSources.h:206-264`；`inc/hgl/graph/RootAddressPush.h:28-64` |
| `MeshDrawParams` 88B 按 DrawBatch 写行 | 几何参数行 112B（24B 头 + 11×uint64，几何创建期写一次，按 `geometry_id` 索引）+ 命令行 `MeshDrawCommand` 8B（`geometry_id` + `first_instance`，每 DrawBatch 一行，按 `gl_DrawID` 索引）；`MaterialBatch::mesh_draw_params_buffer` 现在装的是后者 | `inc/hgl/graph/ShaderBufferSources.h:16-127`；`src/SceneGraph/VKGeometry.cpp:112-176`；`src/ecs/support/PrimitiveBatchPipeline.cpp:591-730`；`inc/hgl/ecs/core/MaterialBatch.h:55-56` |
| `camera_ubo_dirty`（相机 UBO 脏标记） | 字段不存在（0 命中）。`CommitCameraUBO` 每个 RT/RenderPass 开始固定全量写入，不依赖脏标记（`CameraComponent` 侧只有 `matrix_dirty`） | `src/ecs/systems/tick/CameraSystem.cpp:310-319`；`inc/hgl/ecs/components/CameraComponent.h:85` |
| `worldPosition`（RenderItem 上的世界坐标缓存） | 字段不存在（0 命中）。`RenderItem` 只有 `distanceToCamera`（排序用）；世界坐标现取现算（`transform->GetWorldPosition()`） | `inc/hgl/ecs/core/RenderItem.h:40-44`；`src/ecs/systems/render/RenderPrimitiveCollectSystem.cpp:1431-1432` |
| 材质行 `row_id` / `material_texture_*` 命名 | `MaterialComponent` 现为 `data_index_row` + `material_row_cpu/gpu` + `material_texture_configuration` + `material_texture_row_cpu/gpu` + `material_texture_zero_row_gpu` + `material_texture_configuration_hash` | `inc/hgl/ecs/components/MaterialComponent.h:23-35` |
| `RenderTo` / 离屏 pass 相关描述缺失 | 现有一等入口 `ECSContext::RenderTo(const RenderPassRequest&)`（+ `RenderTo(rt, clear, dt)` 便捷重载）与 `RenderPassRequest` / `RenderPassOptions` 结构；`RenderGraph::Pass::renderTarget` 仍**未生效**（非当前 RT 只告警） | `inc/hgl/ecs/core/RenderPassRequest.h:31-64`；`src/ecs/core/Context.cpp:445-557`；`inc/hgl/vk/VKCommandBuffer.h:116-125`；`inc/hgl/ecs/core/RenderGraph.h:38-48` |
| `RenderFrameCache` 里 `componentCount` 与 profiler 组状态 | `RenderFrameCache`（`inc/hgl/ecs/core/Context.h:48-63`）只有 `renderItems` / `materialBatches` / `cameraInfo` / `renderableCount`；`componentCount` 与 SystemProfiler 一并删除 | `inc/hgl/ecs/core/Context.h:48-63`；全仓 0 命中 |
| 逐系统 `[ECS] Update/Render Begin|End` 日志、`[ECS RENDER] ===== Frame Start` | 均已注释掉；现存可观测面 = 组清单日志 + 各系统自身日志（§10） | `src/ecs/core/Context.cpp:824-868`；`src/ecs/core/RenderGraph.cpp:104-176` |
| 「Object 生命周期 `OnCreate/OnUpdate/OnDestroy`」「Tick 循环含每 Entity `OnUpdate()`」 | Object 无虚生命周期钩子；`ECSContext::Tick` 只跑 tick 系统 + `InputSystem::EndFrame()`，每帧全实体 × 全组件的 `OnUpdate` 虚分发已删除 | `inc/hgl/ecs/core/Object.h:54-59`；`src/ecs/core/Context.cpp:290-314` |
| `Context::RegisterDefaultEcsSystems`（Context 内部调用） | 改为应用框架显式调用：`src/Work/AppFramework.cpp:251` | `src/ecs/core/DefaultSystems.cpp:228-245` |
| `Context 构造时预分配 1000` + `RegisterComponentQueryBase` 注册三个基类 | 1000 仍然成立（`src/ecs/core/Context.cpp:62`）；查询基类当前只登记 `PrimitiveComponent` 一个（`src/ecs/core/Context.cpp:88`） | `src/ecs/core/Context.cpp:60-68,88`；`inc/hgl/ecs/core/Context.h:502-514` |
| 组件版本机制 | `Component::version` 仍然存在（`inc/hgl/ecs/core/Component.h:26`，`TouchChange` 时自增 :73），与 `change_mask` 并存；变换侧额外用 `matrixDirty` | `inc/hgl/ecs/core/Component.h:23-79`；`src/ecs/systems/tick/TransformSystem.cpp:457-476` |

**未能核实（本文未改动、未确认，勿引用本文作为其结论）**：

- `Component::version` 是否彻底移除：基类 `Component.h` 无该字段，但未逐一排查各派生组件是否自带同名成员，故 §5 只按基类现状描述。
- `PrimitiveOverlayRenderSystem` 与 `PrimitiveRenderSystem` 的 `GetOverlay()` 分流是否覆盖全部 overlay 场景，未逐一验证（仅核实接口点存在：`src/ecs/support/primitive/PrimitiveRenderSystem.cpp:18-24,72-73`）。
- 未实跑示例，§11 的最后一行数值（`first=0 count=1`）沿用旧基线观测，未在本基线复测。
