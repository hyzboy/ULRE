#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/debug/DebugViewController.h>

#include<utility>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/ecs/core/entity/EntityManager.h>
#include<hgl/ecs/core/DefaultSystems.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/ecs/components/ShadowProxy.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/support/pipeline/PrimitiveState.h>
#include<hgl/ecs/core/render/MaterialBatch.h>
#include<hgl/ecs/core/render/PrimitiveRenderItem.h>
#include<hgl/ecs/support/pipeline/RenderPipelineBase.h>
#include<hgl/ecs/support/transform/TransformAssignmentBuffer.h>
#include<hgl/ecs/support/storage/CameraSlotGuard.h>
#include<hgl/ecs/support/storage/CameraInfoStorage.h>
#include<hgl/ecs/support/material/MaterialAuthoringSideTable.h>
#include<hgl/ecs/support/material/MaterialRuntimePipelineTable.h>
#include<hgl/graph/ubo/WorldAddresses.h>
#include<hgl/vk/buffer/StructView.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/ecs/systems/render/frame/RenderSystemCore.h>
#include<hgl/ecs/systems/render/frame/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/light_shadow/MainLightShadowSystem.h>
#include<hgl/ecs/systems/render/buffer/RenderSceneUBOSystem.h>
#include<hgl/ecs/systems/render/frame/SwapchainNextImageSystem.h>
#include<hgl/ecs/systems/render/frame/SwapchainSubmitSystem.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GraphModuleManager.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/ecs/systems/render/buffer/RenderBufferUploadSystem.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/log/Log.h>
#include<hgl/sync/Mutex.h>
#include<hgl/time/Time.h>
#include<algorithm>
#include<cstdlib>
#include<cstdio>
#include<vector>

namespace hgl
{
    namespace ecs
    {
        namespace
        {
            uint64_t NextRenderDiagnosticWorldID()
            {
                static hgl::Mutex mutex;
                static uint64_t next_id=0;
                hgl::MutexLock lock(&mutex);
                return ++next_id;
            }
        }

        ECSContext::ECSContext(const AnsiString& name)
            : ECSCoreContext(name)
            , render_data(this)
            , gpu_resources(this)
            , render_runner(this)
        {
            render_diagnostic_world_id=NextRenderDiagnosticWorldID();

            // 空间域初始化（绑定 CoreContext/World 指针并注册观察者）
            spatial_state.Initialize(this, this, this);

            // P2：主帧拓扑快照默认开启；宿主可用环境变量（opt-out，`=0` 关闭）或显式 setter 调整。
            render_runner.GetMainFrameTopology().SetSnapshotEnabled(GetMainFrameTopologySnapshotEnvDefault());

            // P3：主帧收集快路径默认开启（独立开关；`IsMainFrameCollectCacheEnabled()`
            // 会额外要求快照本身也开启，所以「P2 关 + P3 开」只会退回完整路径，不会失配）。
            render_runner.GetMainFrameTopology().SetCollectCacheEnabled(GetMainFrameCollectCacheEnvDefault());
        }

        ECSContext::~ECSContext()
        {
            Shutdown();
        }

        bool ECSContext::Initialize(hgl::graph::VulkanDevice* device, hgl::graph::IRenderTarget* target)
        {
            // W3 合并：原 InitializeGraphics 的 GPU 绑定 + 原 Initialize 的系统初始化
            if (!device || !target)
                return false;

            gpu_device = device;
            render_target = target;

            // Env 随世界（C2）：profile 未显式设置时**按需解析本世界 RT 的 env_profile**
            // （见 GetEnvProfileID）——作者侧可能在世界创建之后才 SetEnvironmentProfile，
            // 所以这里不缓存快照。sky / shadow 地址按它写本世界地址表。
            env_profile_explicit = false;
            env_profile_owned = false;

            // 世界私有相机行存储（相机 = 世界级观察者数据）：16 槽 × HGL_FRAME_SLOT_TOTAL 帧槽，
            // 0 号槽恒为本世界默认相机。定稿见 doc/world-addresses-and-camera-model-plan.md §2。
            if (!render_data.InitializeDeviceResources(device, GetName()))
            {
                LogError("[ECSContext::Initialize] 世界相机行存储（CameraInfoStorage）创建失败");
                return false;
            }

            // 世界地址表（WorldAddresses，SSBO + 持久 BDA）：表内是本世界的 world 私有地址，
            // 基址经 pc_root.addr_world_addresses 下发 —— 多世界渲染只换这一个指针。
            if (!InitializeWorldAddressesTable())
            {
                LogError("[ECSContext::Initialize] 世界地址表（WorldAddresses）创建失败");
                return false;
            }

            // Propagate device to RenderBufferUploadSystem if it was registered first
            if (auto upload_system = GetSystem<RenderBufferUploadSystem>())
                upload_system->SetDevice(gpu_device);

            // A5b：渲染组件删除后，图元枚举改按 GeometryData（精确类型）——
            // 不再需要基类查询注册（原图元组件的基类查询注册已随该类删除）。

            // Ensure TransformSystem is registered and bound to this world
            {
                auto transform_system = GetSystem<TransformSystem>();
                if (!transform_system)
                {
                    transform_system = RegisterSystem<TransformSystem>();
                }

                if (transform_system)
                {
                    transform_system->SetWorld(this);
                }
            }

            // Ensure RenderBufferUploadSystem is always present — it is infrastructure,
            // not a feature system. Every app with staged GPU buffers needs it.
            {
                auto upload_system = GetSystem<RenderBufferUploadSystem>();
                if (!upload_system)
                    upload_system = RegisterSystem<RenderBufferUploadSystem>();

                if (upload_system)
                {
                    upload_system->SetWorld(this);
                    upload_system->SetDevice(gpu_device);
                }
            }

                system_registry.SortTick();
                system_registry.SortRender();

            // Initialize all systems
                for (auto& entry : system_registry.TickOrder())
                {
                    if (entry.system)
                    {
                        entry.system->Initialize();
                    }
                }

                for (auto& entry : system_registry.RenderOrder())
                {
                    if (entry.system)
                    {
                        entry.system->Initialize();
                    }
                }

            active = true;
            return true;
        }

        std::shared_ptr<CameraSystem> ECSContext::EnsureCameraSystem()
        {
            auto camera_system = GetSystem<CameraSystem>();
            if (!camera_system)
            {
                camera_system = RegisterSystem<CameraSystem>(this);
            }

            if (!camera_system)
                return nullptr;

            if (active)
            {
                camera_system->Initialize();
            }

            return camera_system;
        }

        void ECSContext::ClearPipelines()
        {
            for (auto& pair : render_pipelines)
            {
                delete pair.second;
            }
            render_pipelines.Clear();
        }

        RenderPipelineBase* ECSContext::GetRenderPipeline(const char* name)
        {
            if (!name) return nullptr;
            return GetRenderPipeline(AnsiString(name));
        }

        RenderPipelineBase* ECSContext::GetRenderPipeline(const std::string& name)
        {
            return GetRenderPipeline(AnsiString(name.c_str()));
        }

        RenderPipelineBase* ECSContext::GetRenderPipeline(const AnsiString& name)
        {
            auto* ptr = render_pipelines.GetValuePointer(name);
            return ptr ? *ptr : nullptr;
        }

        void ECSContext::RegisterRenderPipeline(const char* name, std::unique_ptr<RenderPipelineBase> pipeline)
        {
            if (!name || !pipeline) return;
            RegisterRenderPipeline(AnsiString(name), std::move(pipeline));
        }

        void ECSContext::RegisterRenderPipeline(const std::string& name, std::unique_ptr<RenderPipelineBase> pipeline)
        {
            RegisterRenderPipeline(AnsiString(name.c_str()), std::move(pipeline));
        }

        void ECSContext::RegisterRenderPipeline(const AnsiString& name, std::unique_ptr<RenderPipelineBase> pipeline)
        {
            if (!pipeline)
                return;

            auto* old = render_pipelines.GetValuePointer(name);
            if (old && *old)
                delete *old;

            render_pipelines[name] = pipeline.release();
        }

        void ECSContext::Shutdown()
        {
            if (shutdown_in_progress)
                return;

            shutdown_in_progress = true;

            DebugViewController::Reset(this);

            if (auto *device = GetGPUDevice())
                device->WaitIdle();

            // Release support pipelines early while graphics managers are still valid.
            // AppFramework destroys GraphicsContext before deleting ECSContext, so
            // deferring this to ECSContext destructor can access dangling pointers.
            // Text/Primitive specific support-pipeline members removed; all pipelines live in render_pipelines.

            // Release all registered render pipelines
            ClearPipelines();

            // Release render-frame items first  (only clears renderItems, keeps materialBatches for reuse)
            render_runner.ClearFrameCache();

            if (!active)
            {
                // Even when inactive, clear entities/components now so component OnDetach
                // does not run later during member destruction after maps are already destroyed.
                ClearEntities();

                system_registry.ClearRegistrations();
                ClearComponentPools();
                system_registry.ClearElementIndex();
                system_registry.ClearDependencies();
                ResetComponentDrivenGroups();
                spatial_state.Clear();

                LogDebug("[ECSContext] Shutdown(inactive) - releasing material batches");
                render_runner.ClearMaterialBatches();

                render_data.Clear();
                render_runner.Shutdown();

                shutdown_in_progress = false;
                return;
            }

            // 先标记 inactive，避免系统/组件在 Shutdown 过程中触发重入时再走完整清理路径
            active = false;

            // Destroy all systems FIRST(before clearing materialBatches)
            system_registry.ShutdownSystems();

            // Destroy all entities
            ClearEntities();

            ClearComponentPools();
            ResetComponentDrivenGroups();
            spatial_state.Clear();

            // Finally, clear materialBatches after all systems/entities are destroyed
            LogDebug("[ECSContext] Shutdown - releasing material batches");
            render_runner.ClearMaterialBatches();
            LogDebug("[ECSContext] Shutdown - material batches cleared");

            render_data.Clear();
            render_runner.Shutdown();

            // 相机槽拥有者的安全网由 **弱引用** 提供：`CameraSlotGuard` 只持
            // `weak_ptr<CameraInfoStorage>`，世界先销毁时 lock() 失败 ⇒ 析构自动 no-op，不需要在这里
            // 逐个 Detach（世界可能从未 Initialize，Shutdown 会走早退分支 —— 那正是悬垂指针的来源）。
            // 槽本身也不需要逐个归还：`camera_info_storage->Reset()` 会把槽账目整表清空。

            // Env 随世界（C2）：**本世界创建/拥有的** profile 在这里归还（内置 default 永不归还）。
            // 释放顺序在世界地址表之前——表里还留着它的地址，但表随后即销毁。
            if (env_profile_owned)
            {
                if (auto *env_manager = ResolveEnvManager())
                    env_manager->Release(env_profile);
            }
            env_profile = graph::kEnvProfileDefault;
            env_profile_explicit = false;
            env_profile_owned = false;

            // 世界地址表释放（StructView 非拥有 ⇒ buffer 走 BufferManager 归还）
            if (auto *gc = GetGraphicsContext())
            {
                if (auto *bm = gc->GetBufferManager())
                {
                    gpu_resources.Release(bm);
                }
            }
            else
            {
                gpu_resources.Release(nullptr);
            }

            shutdown_in_progress = false;
        }

        void ECSContext::Tick(float deltaTime)
        {
            if (!active)
                return;

            system_registry.SortTick();

            // Update non-render systems
            for (auto& entry : system_registry.TickOrder())
            {
                if (entry.system)
                    RunSystemUpdate(entry.system.get(), deltaTime);
            }

            // 原每帧全实体 × 全组件的 OnUpdate 虚分发已删除——其唯一有效实现
            // （fixed_pixel 等像素缩放）由 gizmo 的
            // TransformGizmoSystem 在 TickPostCamera 相位经
            // SetFixedPixelSizingContextAndApply 即时重算（本就是主通道）。
            // 逻辑更新归 tick 系统，不归组件。

            if (auto input_system = GetSystem<InputSystem>())
            {
                input_system->EndFrame();
            }
        }

        hgl::graph::GraphicsContext* ECSContext::GetGraphicsContext()
        {
            if (!graphics_context && render_context)
                graphics_context = render_context->GetGraphicsContext();
            return graphics_context;
        }

        const hgl::graph::GraphicsContext* ECSContext::GetGraphicsContext() const
        {
            if (graphics_context)
                return graphics_context;
            return render_context ? render_context->GetGraphicsContext() : nullptr;
        }

        void ECSContext::SetFrameIndex(const uint32_t index)
        {
            render_runner.SetFrameIndex(index);

            // 世界私有直推：只推进本世界 TransformSystem 的 L2W ring 帧索引
            // （旧静态广播会触达所有世界的 buffer，且静态表析构不摘除留悬空）
            auto ts = GetSystem<TransformSystem>();
            if (ts)
                if (auto *tb = ts->GetTransformBuffer())
                    tb->SetFrameIndex(index);

            // 帧槽变了 ⇒ 本世界的地址表换到本槽（表内各列随世界，
            // 每个 pass 各写自己的槽，不与在途帧/其它世界互踩）。
            SyncWorldAddresses();
        }

        uint32_t ECSContext::GetActiveCameraRow() const
        {
            // 槽号非法（本帧尚未解析出相机认领的槽 / 槽耗尽）⇒ 退回 **0 号槽**（默认相机 / 常驻 fallback）：
            // 着色器拿到的永远是一个合法行，而不是按 INVALID_SLOT 算出的越界行（会读到别的世界的存储）。
            const uint32_t slot = CameraInfoStorage::IsValidSlot(render_runner.GetPassScopeState().GetCameraSlot())
                                    ? render_runner.GetPassScopeState().GetCameraSlot()
                                    : CameraInfoStorage::kDefaultCameraSlot;

            // 行号 = 相机槽 × 帧槽总数 + 帧槽（世界内算术，见 CameraInfoStorage::CameraRow）
            return CameraInfoStorage::CameraRow(slot, render_runner.GetFrameIndex());
        }

        const graph::CameraInfo* ECSContext::GetActiveCameraInfo()
        {
            auto camera_system = GetSystem<CameraSystem>();
            return camera_system ? camera_system->GetActiveCameraInfo() : nullptr;
        }

        const graph::CameraInfo* ECSContext::ResolveActiveCameraInfo() const
        {
            // §3-⑥：纯读转发（不认领、不造 fallback）
            auto camera_system = GetSystem<CameraSystem>();
            return camera_system ? camera_system->ResolveActiveCameraInfo() : nullptr;
        }

        // ==== 相机（世界级资源：16 槽 × 帧槽，0 号槽恒为本世界默认相机）====
        // 「相机属于哪个世界 / 哪个槽」的唯一真源 = camera_info_storage 的槽账目（槽 → 宿主弱引用）。

        CameraComponent* ECSContext::GetCamera(const uint32_t slot) const
        {
            return render_data.GetCamera(slot);
        }

        CameraComponent* ECSContext::GetDefaultCamera() const
        {
            return render_data.GetDefaultCamera();
        }

        bool ECSContext::SetDefaultCamera(const std::shared_ptr<CameraComponent>& camera)
        {
            return render_data.SetDefaultCamera(camera);
        }

        uint32_t ECSContext::GetCameraSlot(const CameraComponent* camera) const
        {
            return render_data.GetCameraSlot(camera);
        }

        namespace
        {
            // 世界级「实体 → 组件」访问器的**核心**：调用方手里已有实体（`Entity*`）时走这里，
            // 不再做 `GetEntity(EntityID)` 那次多余往返 —— `EntityID` 版只负责解析一次后转调。
            // 两种句柄是**不同的句柄，不是兼容层**：ID 版给只持有 ID 的引擎内部代码，
            // 指针版给持有实体的作者/系统代码。
            template<typename T>
            inline T *ComponentOfEntity(Entity *entity)
            {
                return entity ? entity->GetComponent<T>() : nullptr;
            }

            template<typename T>
            inline const T *ComponentOfEntity(const Entity *entity)
            {
                return entity ? entity->GetComponent<T>() : nullptr;
            }

            template<typename T, typename... Args>
            inline T *GetOrAddComponentOfEntity(Entity *entity, Args&&... args)
            {
                if (!entity)
                    return nullptr;

                T *component = entity->GetComponent<T>();

                if (!component)
                    component = entity->AddComponent<T>(std::forward<Args>(args)...);

                return component;
            }
        }

        // 实体 → 相机（世界级读取口；与 GetMaterialData / GetGeometryData 同族）。
        // 相机是**世界级资源**（槽账目在 camera_info_storage），实体只是宿主：
        // 示例/系统取相机一律走这里。
        //
        // §3-②（2026-10-11）：这四个口与 `GetOrCreateCamera` **不再**经 `ComponentOfEntity<T>` /
        // `GetOrAddComponentOfEntity<T>`（那两者内部用 `Entity::GetComponent<T>` / `AddComponent<T>`，
        // 而这两个模板已对 `CameraComponent` **编译期禁死**）—— 改走同一套"行号 → 池行"路径
        // （`Entity::GetComponentRow` + `ECSContext::GetComponentByRow`），语义逐字一致。
        const CameraComponent* ECSContext::GetCameraByEntity(const EntityID owner) const
        {
            const Entity *entity = GetEntity(owner);

            return entity ? GetComponentByRow<CameraComponent>(
                                entity->GetComponentRow(ComponentTypeOf<CameraComponent>::value))
                          : nullptr;
        }

        CameraComponent* ECSContext::GetCameraByEntity(const EntityID owner)
        {
            Entity *entity = GetEntity(owner);

            return entity ? GetComponentByRow<CameraComponent>(
                                entity->GetComponentRow(ComponentTypeOf<CameraComponent>::value))
                          : nullptr;
        }

        const CameraComponent* ECSContext::GetCameraByEntity(const Entity *owner) const
        {
            return owner ? GetComponentByRow<CameraComponent>(
                               owner->GetComponentRow(ComponentTypeOf<CameraComponent>::value))
                         : nullptr;
        }

        CameraComponent* ECSContext::GetCameraByEntity(Entity *owner)
        {
            return owner ? GetComponentByRow<CameraComponent>(
                               owner->GetComponentRow(ComponentTypeOf<CameraComponent>::value))
                         : nullptr;
        }

        CameraComponent* ECSContext::GetOrCreateCamera(const EntityID owner, const std::string& name)
        {
            return GetOrCreateCamera(GetEntity(owner), name);
        }

        CameraComponent* ECSContext::GetOrCreateCamera(Entity *owner, const std::string& name)
        {
            // §3-②（2026-10-11）：相机**唯一创建入口** —— 不再经 `Entity::AddComponent<CameraComponent>`
            // （已编译期禁死），改走世界侧的 `CreateComponent<CameraComponent>`（同一挂载路径）。
            if (!owner)
                return nullptr;

            if (CameraComponent *existing = GetCameraByEntity(owner))
                return existing;

            return CreateComponent<CameraComponent>(owner, name);
        }

        bool ECSContext::ReleaseCameraSlot(const CameraComponent* camera)
        {
            return render_data.ReleaseCameraSlot(camera);
        }

        bool ECSContext::EnsureCameraSlot(CameraComponent* camera, const bool is_default)
        {
            return render_data.EnsureCameraSlot(camera, is_default);
        }

        CameraComponent* ECSContext::EnsureFallbackCamera()
        {
            return render_data.EnsureFallbackCamera();
        }

        // ── T2-c：组件行 → 系统组计数（组归属是**类型属性**，取自类型表）──
        void ECSContext::OnComponentRowAdded(const ComponentType type)
        {
            // P1：组件挂载改变「实体具备哪些能力」⇒ 主帧图元的候选集合可能变化。
            // 必须放在下面的早退**之前**：`SystemGroupID::None` 的组件同样可能被
            // 图元侧读到（如 GPUModelRefComponent 只做数据不建系统组）。
            MarkTopologyChanged(TopologyChange::EntityStructure);

            const SystemGroupID group = GetComponentGroup(type);

            if (group == SystemGroupID::None)
                return;

            system_group_activity.Add(group);

            EnsureSystemGroupSystems(this, group, GetRenderTarget());
            SetSystemGroupEnabled(group, true);
        }

        void ECSContext::OnComponentRowRemoved(const ComponentType type)
        {
            // P1：同 `OnComponentRowAdded`，早退之前记账（卸载同样改变候选集合）。
            MarkTopologyChanged(TopologyChange::EntityStructure);

            const SystemGroupID group = GetComponentGroup(type);

            if (group == SystemGroupID::None)
                return;

            if (system_group_activity.Remove(group))
                SetSystemGroupEnabled(group, false);
        }

        void ECSContext::OnEntityDestroying(const EntityID id)
        {
            spatial_state.OnEntityDestroy(id);
        }

        void ECSContext::OnEntityDestroyed(const EntityID id)
        {
            DeactivateEntityRow(id);
            render_data.DestroyEntityData(id);
            MarkTopologyChanged(TopologyChange::EntityStructure);
        }

        void ECSContext::OnEntityDeactivated(const EntityID id)
        {
            DeactivateEntityRow(id);
        }

        void ECSContext::OnEntityRowRefreshed(Entity *entity)
        {
            RefreshEntityRow(entity);
        }

        void ECSContext::OnComponentRowDestroyed(const ComponentType type, const uint32_t row)
        {
            DestroyComponent(type, row);
        }

        void ECSContext::OnSceneStructureDirty()
        {
            MarkSceneStructureDirty();
        }

        void ECSContext::OnTransformRegistered(TransformID id, bool isMovable)
        {
            // 有变换出现即自动装上 TransformSystem（避免样例手动注册）
            auto transform_system = GetSystem<TransformSystem>();
            if (!transform_system)
                transform_system = RegisterSystem<TransformSystem>();

            if (transform_system)
            {
                transform_system->SetWorld(this);
                transform_system->SetEnabled(true);
            }
        }

        uint32_t ECSContext::GetTransformSegmentBudget(const bool movable) const
        {
            return spatial_state.GetTransformSegmentBudget(movable);
        }

        uint32_t ECSContext::AcquireTransformSlot(const bool movable)
        {
            return spatial_state.AcquireTransformSlot(movable);
        }

        void ECSContext::ReleaseTransformSlot(const bool movable,const uint32_t slot)
        {
            spatial_state.ReleaseTransformSlot(movable, slot);
        }

        bool ECSContext::TryGetTransformSlot(TransformID id,bool movable,uint32_t &out_slot) const
        {
            return spatial_state.TryGetTransformSlot(id, movable, out_slot);
        }

        uint32_t ECSContext::GetLiveTransformCount(const bool movable) const
        {
            return spatial_state.GetLiveTransformCount(movable);
        }

        bool ECSContext::RegisterTransform(TransformID id,bool isMovable)
        {
            return spatial_state.RegisterTransform(id, isMovable);
        }

        bool ECSContext::MigrateTransform(TransformID id,bool toMovable)
        {
            return spatial_state.MigrateTransform(id, toMovable);
        }

        void ECSContext::UnregisterTransform(TransformID id)
        {
            spatial_state.UnregisterTransform(id);
        }

        TransformID ECSContext::CreateTransform(EntityID owner,Mobility mobility)
        {
            return spatial_state.CreateTransform(owner, mobility);
        }

        void ECSContext::DestroyTransform(TransformID id)
        {
            spatial_state.DestroyTransform(id);
        }

        TransformID ECSContext::GetTransformID(EntityID owner) const
        {
            return spatial_state.GetTransformID(owner);
        }

        TransformAccessor ECSContext::GetTransform(TransformID id)
        {
            return spatial_state.GetTransform(id);
        }

        BoundingBoxAccessor ECSContext::GetBoundingBoxByEntity(EntityID owner) const
        {
            return spatial_state.GetBoundingBoxByEntity(owner);
        }

        BoundingBoxAccessor ECSContext::GetOrCreateBoundingBox(EntityID owner)
        {
            return spatial_state.GetOrCreateBoundingBox(owner);
        }

        MaterialData* ECSContext::GetMaterialData(EntityID owner)
        {
            return render_data.GetMaterialData(owner);
        }

        const MaterialData* ECSContext::GetMaterialData(EntityID owner) const
        {
            return render_data.GetMaterialData(owner);
        }

        MaterialData* ECSContext::GetMaterialData(Entity *owner)
        {
            return render_data.GetMaterialData(owner);
        }

        const MaterialData* ECSContext::GetMaterialData(const Entity *owner) const
        {
            return render_data.GetMaterialData(owner);
        }

        MaterialData* ECSContext::GetOrCreateMaterialData(EntityID owner)
        {
            return render_data.GetOrCreateMaterialData(owner);
        }

        MaterialData* ECSContext::GetOrCreateMaterialData(Entity *owner)
        {
            return render_data.GetOrCreateMaterialData(owner);
        }

        GeometryData* ECSContext::GetGeometryData(EntityID owner)
        {
            return ComponentOfEntity<GeometryData>(GetEntity(owner));
        }

        const GeometryData* ECSContext::GetGeometryData(EntityID owner) const
        {
            return ComponentOfEntity<GeometryData>(GetEntity(owner));
        }

        GeometryData* ECSContext::GetGeometryData(Entity *owner)
        {
            return ComponentOfEntity<GeometryData>(owner);
        }

        const GeometryData* ECSContext::GetGeometryData(const Entity *owner) const
        {
            return ComponentOfEntity<GeometryData>(owner);
        }

        GeometryData* ECSContext::GetOrCreateGeometryData(EntityID owner)
        {
            return GetOrAddComponentOfEntity<GeometryData>(GetEntity(owner));
        }

        GeometryData* ECSContext::GetOrCreateGeometryData(Entity *owner)
        {
            return GetOrAddComponentOfEntity<GeometryData>(owner);
        }

        void ECSContext::InvalidateEntityRuntimePipeline(EntityID owner)
        {
            // B5-b：管线缓存已迁到世界级旁表（slot 里不放指针）
            render_data.InvalidateEntityRuntimePipeline(owner);

            // P1：解析出的 shader/管线身份失效 ⇒ 材质批的键可能变（拓扑类别）。
            MarkTopologyChanged(TopologyChange::MaterialBinding);
        }

        void ECSContext::DestroyBoundingBox(BoundingBoxDataStorage::HandleID id)
        {
            spatial_state.DestroyBoundingBox(id);
        }
    }//namespace ecs
}//namespace hgl