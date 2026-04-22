#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/EntityManager.h>
#include<hgl/ecs/core/DefaultSystems.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/VisibilitySystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/components/SubSceneMembershipComponent.h>
#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
// TextRenderPipeline.h removed — now owned by TextRenderPipelineAdapter
// LineRenderSystem.h removed — LineRenderSystem is now in support/line/
#include<hgl/ecs/support/RenderPipelineBase.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
// old systems/render/LineRenderSystem.h removed — replaced by support/line/LineRenderSystem
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/systems/render/SwapchainNextImageSystem.h>
#include<hgl/ecs/systems/render/SwapchainSubmitSystem.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include<hgl/log/Log.h>
#include<hgl/object/ObjectTracker.h>
#include<algorithm>
#include<chrono>

namespace
{
    struct SubWorldDispatchStats
    {
        uint32_t shared_count = 0;
        uint32_t isolated_count = 0;
        uint32_t dispatched_count = 0;
    };

    template<typename Fn>
    SubWorldDispatchStats DispatchLocalLogicSubWorldComponents(hgl::ecs::ECSContext* context, Fn&& fn)
    {
        SubWorldDispatchStats stats;

        if (!context)
            return stats;

        std::vector<std::shared_ptr<hgl::ecs::SubWorldComponent>> sub_worlds;
        context->GetComponents(sub_worlds);
        for (const auto& sub_world : sub_worlds)
        {
            if (!sub_world)
                continue;

            if (!sub_world->IsLogicIsolated())
            {
                ++stats.shared_count;
                continue;
            }

            ++stats.isolated_count;
            fn(*sub_world);
            ++stats.dispatched_count;
        }

        return stats;
    }

    template<typename Fn>
    SubWorldDispatchStats DispatchLocalRenderSubWorldComponents(hgl::ecs::ECSContext* context, Fn&& fn)
    {
        SubWorldDispatchStats stats;

        if (!context)
            return stats;

        std::vector<std::shared_ptr<hgl::ecs::SubWorldComponent>> sub_worlds;
        context->GetComponents(sub_worlds);
        for (const auto& sub_world : sub_worlds)
        {
            if (!sub_world)
                continue;

            if (sub_world->IsRenderShared())
            {
                ++stats.shared_count;
                continue;
            }

            ++stats.isolated_count;
            fn(*sub_world);
            ++stats.dispatched_count;
        }

        return stats;
    }
}

namespace hgl
{
    namespace ecs
    {
        RenderFrameCache::~RenderFrameCache() = default;

        void RenderFrameCache::BeginFrame()
        {
            renderItems.clear();
            renderableCount = 0;

            // 清空批次内容，保留对象以供重用
            for (auto& pair : materialBatches)
            {
                if (pair.second)
                    pair.second->Clear();
            }
        }

        ECSContext::ECSContext(const std::string& name)
            : Object(name)
            , entity_manager(std::make_unique<EntityManager>(1000))
            , active(false)
        {
        }

        ECSContext::~ECSContext()
        {
            Shutdown();
        }

        bool ECSContext::InitializeGraphics(hgl::graph::VulkanDevice* device, hgl::graph::IRenderTarget* target) {
            if (!device || !target) {
                // Phase 1 debug: device or target is null
                return false;
            }

            gpu_device = device;
            render_target = target;

            // Propagate device to RenderBufferUploadSystem if it was registered first
            if (auto upload_system = GetSystem<RenderBufferUploadSystem>())
                upload_system->SetDevice(gpu_device);

            // Phase 1: Initialized with GPU device and render target
            return true;
        }

        void ECSContext::Initialize()
        {
            RegisterComponentQueryBase<RenderableComponent>();
            RegisterComponentQueryBase<PrimitiveComponent>();

            // Ensure TransformSystem is registered and bound to this world
            {
                auto transform_system = GetSystem<TransformSystem>();
                if (!transform_system)
                {
                    transform_system = RegisterTickSystem<TransformSystem>();
                }

                if (transform_system)
                {
                    transform_system->SetWorld(this);
                }
            }

            // Ensure VisibilitySystem is registered
            {
                auto visibility_system = GetSystem<VisibilitySystem>();
                if (!visibility_system)
                {
                    visibility_system = RegisterTickSystem<VisibilitySystem>();
                }

                if (visibility_system)
                {
                    visibility_system->SetWorld(this);
                    // VulkanDevice will be set later when available
                }
            }

            // Ensure RenderBufferUploadSystem is always present — it is infrastructure,
            // not a feature system. Every app with staged GPU buffers needs it.
            {
                auto upload_system = GetSystem<RenderBufferUploadSystem>();
                if (!upload_system)
                    upload_system = RegisterRenderSystem<RenderBufferUploadSystem>();

                if (upload_system)
                {
                    upload_system->SetWorld(this);
                    upload_system->SetDevice(gpu_device); // may be null here for deferred init; set again in InitializeGraphics
                }
            }

                SortTickSystems();
                SortRenderSystems();

            // Initialize all systems
                for (auto& entry : tick_system_order)
                {
                    if (entry.system)
                    {
                        entry.system->OnDependenciesReady();
                        entry.system->Initialize();
                    }
                }

                for (auto& entry : render_system_order)
                {
                    if (entry.system)
                    {
                        entry.system->OnDependenciesReady();
                        entry.system->Initialize();
                    }
                }

            active = true;
            OnCreate();
        }

        Entity* ECSContext::CreateChildEntity(Entity *parent,
                                               const ChildEntityDesc &desc,
                                               std::vector<EntityID> *out_entity_ids,
                                               std::shared_ptr<TransformComponent> *out_transform)
        {
            if (!parent)
                return nullptr;

            auto *entity = CreateEntity<Entity>(desc.name ? desc.name : "Entity");
            if (!entity)
                return nullptr;

            auto transform = entity->AddComponent<TransformComponent>(desc.mobility);
            if (!transform)
                return nullptr;

            transform->SetLocalTRS(desc.position, desc.rotation, desc.scale);
            transform->SetParent(parent->GetID());

            if (out_transform)
                *out_transform = transform;

            if (out_entity_ids)
                out_entity_ids->push_back(entity->GetID());

            return entity;
        }

        std::shared_ptr<CameraSystem> ECSContext::EnsureCameraSystem()
        {
            auto camera_system = GetSystem<CameraSystem>();
            if (!camera_system)
            {
                camera_system = RegisterTickSystem<CameraSystem>(this);
            }

            if (!camera_system)
                return nullptr;

            if (active)
            {
                camera_system->OnDependenciesReady();
                camera_system->Initialize();
            }

            return camera_system;
        }

        uint64_t ECSContext::ResolveEntitySubsceneID(const Entity* entity) const
        {
            if (!entity)
                return 0;

            auto membership = entity->GetComponent<SubSceneMembershipComponent>();
            if (!membership)
                return 0;

            return membership->GetSubsceneID();
        }

        void ECSContext::SetSubsceneState(uint64_t subscene_id, bool paused, bool tick_enabled, bool render_enabled)
        {
            if (subscene_id == 0)
                return;

            SubSceneState state;
            state.paused = paused;
            state.tick_enabled = tick_enabled;
            state.render_enabled = render_enabled;
            subscene_states[subscene_id] = state;

#if ULRE_ECS_DEBUG_API
            LogDebug("[ECSContext] SetSubsceneState context='%s' subscene=%llu paused=%d tick=%d render=%d size=%zu",
                     GetName().c_str(),
                     static_cast<unsigned long long>(subscene_id),
                     paused ? 1 : 0,
                     tick_enabled ? 1 : 0,
                     render_enabled ? 1 : 0,
                     subscene_states.size());
#endif
        }

        bool ECSContext::GetSubsceneState(uint64_t subscene_id, SubSceneState& out_state) const
        {
            auto it = subscene_states.find(subscene_id);
            if (it == subscene_states.end())
                return false;

            out_state = it->second;
            return true;
        }

        void ECSContext::RemoveSubsceneState(uint64_t subscene_id)
        {
            if (subscene_id == 0)
                return;

            const size_t erased = subscene_states.erase(subscene_id);

#if ULRE_ECS_DEBUG_API
            LogDebug("[ECSContext] RemoveSubsceneState context='%s' subscene=%llu erased=%zu size=%zu shutdown=%d",
                     GetName().c_str(),
                     static_cast<unsigned long long>(subscene_id),
                     erased,
                     subscene_states.size(),
                     shutdown_in_progress ? 1 : 0);
#endif
        }

        bool ECSContext::IsSubsceneTickEnabled(uint64_t subscene_id) const
        {
            if (subscene_id == 0)
                return true;

            auto it = subscene_states.find(subscene_id);
            if (it == subscene_states.end())
                return true;

            const auto& state = it->second;
            return !state.paused && state.tick_enabled;
        }

        bool ECSContext::IsSubsceneRenderEnabled(uint64_t subscene_id) const
        {
            if (subscene_id == 0)
                return true;

            auto it = subscene_states.find(subscene_id);
            if (it == subscene_states.end())
                return true;

            const auto& state = it->second;
            return !state.paused && state.render_enabled;
        }

        bool ECSContext::IsEntityTickEnabled(const Entity* entity) const
        {
            return IsSubsceneTickEnabled(ResolveEntitySubsceneID(entity));
        }

        bool ECSContext::IsEntityRenderEnabled(const Entity* entity) const
        {
            return IsSubsceneRenderEnabled(ResolveEntitySubsceneID(entity));
        }

        RenderPipelineBase* ECSContext::GetRenderPipeline(const std::string& name)
        {
            auto it = render_pipelines.find(name);
            if (it != render_pipelines.end())
                return it->second.get();
            return nullptr;
        }

        void ECSContext::RegisterRenderPipeline(const std::string& name, std::unique_ptr<RenderPipelineBase> pipeline)
        {
            if (!pipeline)
                return;

            LogDebug("[ECS] Registering render pipeline: %s", name.c_str());
            render_pipelines[name] = std::move(pipeline);
        }

        bool ECSContext::IsRenderPipelineEnabled(const std::string& name) const
        {
            auto it = render_pipelines.find(name);
            if (it != render_pipelines.end())
                return it->second != nullptr;  // If registered and not null, it's enabled
            return false;
        }

        std::vector<std::string> ECSContext::GetRenderPipelineNames() const
        {
            std::vector<std::string> names;
            names.reserve(render_pipelines.size());
            for (const auto& pair : render_pipelines)
                names.push_back(pair.first);
            return names;
        }

        void ECSContext::Shutdown()
        {
            if (shutdown_in_progress)
                return;

            shutdown_in_progress = true;

            if (auto *device = GetGPUDevice())
                device->WaitIdle();

            // Release support pipelines early while graphics managers are still valid.
            // AppFramework destroys GraphicsContext before deleting ECSContext, so
            // deferring this to ECSContext destructor can access dangling pointers.
            // Text/Primitive specific support-pipeline members removed; all pipelines live in render_pipelines.
            
            // Release all registered render pipelines
            render_pipelines.clear();

            // Release render-frame items first  (only clears renderItems, keeps materialBatches for reuse)
            render_frame_cache.renderItems.clear();
            render_frame_cache.cameraInfo = nullptr;
            render_frame_cache.renderableCount = 0;

            if (!active)
            {
                // Even when inactive, clear entities/components now so component OnDetach
                // does not run later during member destruction after maps are already destroyed.
                if (entity_manager)
                {
                    LogDebug("[ECSContext] Shutdown(inactive) - clearing entities before member teardown");
                    entity_manager->Clear();
                }

                tick_systems.Clear();
                render_systems.Clear();
                tick_system_order.clear();
                render_system_order.clear();
                component_registry.Clear();
                systems_by_element_type.clear();
                tick_dependencies.Clear();
                render_dependencies.Clear();
                system_group_component_counts.clear();
                known_system_groups.clear();
                installed_system_groups.clear();
                static_transforms.clear();
                movable_transforms.clear();
                subscene_states.clear();

                LogDebug("[ECSContext] Shutdown(inactive) - releasing %zu material batches",
                         render_frame_cache.materialBatches.GetCount());
                render_frame_cache.materialBatches.Clear();
                shutdown_in_progress = false;
                return;
            }

            // 先标记 inactive，避免系统/组件在 Shutdown 过程中触发重入时再走完整清理路径
            active = false;

            // Destroy all systems FIRST(before clearing materialBatches)
            SortTickSystems();
            SortRenderSystems();

            for (auto& entry : tick_system_order)
            {
                if (entry.system)
                    entry.system->Shutdown();
            }
            tick_system_order.clear();

            for (auto& entry : render_system_order)
            {
                if (entry.system)
                    entry.system->Shutdown();
            }
            render_system_order.clear();

            tick_systems.Clear();
            render_systems.Clear();
            global_render_system_count = 0;
            local_gameplay_system_count = 0;

            // Destroy all entities
            if (entity_manager)
            {
                entity_manager->Clear();
            }

            component_registry.Clear();
            system_group_component_counts.clear();
            known_system_groups.clear();
            installed_system_groups.clear();
            static_transforms.clear();
            movable_transforms.clear();
            subscene_states.clear();

            // Finally, clear materialBatches after all systems/entities are destroyed
            LogDebug("[ECSContext] Shutdown - releasing %zu material batches",
                     render_frame_cache.materialBatches.GetCount());
            render_frame_cache.materialBatches.Clear();
            LogDebug("[ECSContext] Shutdown - material batches cleared");
            OnDestroy();
            shutdown_in_progress = false;
        }

        void ECSContext::Tick(float deltaTime)
        {
            if (!active)
                return;

            filtered_entity_count_last_frame = 0;

            SortTickSystems();

            // Update non-render systems
            for (auto& entry : tick_system_order)
            {
                if (entry.system)
                    RunSystemUpdate(entry.system.get(), deltaTime);
            }

            // Update all entities
            if (entity_manager)
            {
                std::vector<Entity*> entities;
                entity_manager->GetAllEntityPointers(entities);
                for (auto entity : entities)
                {
                    if (entity)
                    {
                        if (!IsEntityTickEnabled(entity))
                        {
                            ++filtered_entity_count_last_frame;
                            continue;
                        }

                        entity->OnUpdate(deltaTime);
                    }
                }
            }

            // Update sub-worlds attached via SubWorldComponent
            if (sub_world_auto_update)
            {
                const auto stats = DispatchLocalLogicSubWorldComponents(
                    this,
                    [deltaTime](SubWorldComponent& sub_world)
                    {
                        sub_world.UpdateSubWorld(deltaTime);
                    });

#if ULRE_ECS_DEBUG_API
                static bool logged_once = false;
                if (!logged_once && stats.shared_count > 0)
                {
                    logged_once = true;
                    LogDebug("[ECSContext] Tick auto sub-world dispatch: shared=%u isolated=%u dispatched=%u",
                             stats.shared_count,
                             stats.isolated_count,
                             stats.dispatched_count);
                }
#endif
            }

            if (auto input_system = GetSystem<InputSystem>())
            {
                input_system->EndFrame();
            }
        }

        void ECSContext::Render(graph::RenderCmdBuffer *cmd, float deltaTime)
        {
            if (!active)
                return;

            // Boundary rule (Phase 1): this overload executes render-phase ECS systems
            // for an already-open frame command buffer. The frame lifecycle driver is
            // ECSContext::Render(float) -> RenderSystemCore.
            static bool warned_missing_cmd_once = false;
            if (!cmd && !current_render_cmd && !warned_missing_cmd_once)
            {
                LogWarning("[ECSContext::Render(cmd)] called without command buffer. "
                           "Preferred entry is ECSContext::Render(float) frame driver path.");
                warned_missing_cmd_once = true;
            }

            // (Phase 1) 设置当前渲染命令缓冲区（如果没有由 RenderSystemCore 设置）
            if (!current_render_cmd && cmd) {
                current_render_cmd = cmd;
            }

            // Run CPU-side Update() for Collect and Batch phases only.
            // Buffer commit/upload (phases RenderBufferCommit, RenderBufferUpload) and
            // FrameSync must NOT run inside an open Vulkan render pass — they are
            // handled by PrepareRenderPassSetup() before BeginRenderPass().
            // DrawSubmit systems (RenderPrimitiveSubmitSystem etc.) only override
            // Render(), not Update(), so they are covered by the Render() loop below.

            // SubmitTransformUpdates() MUST run before RenderBatch phase (PrimitiveBuildSystem),
            // because WriteItems() reads world matrices from TransformDataStorage. If batching
            // runs first, static transform world matrices are still identity (never computed),
            // causing a first-frame bug where all static entities render at world origin.
            if (auto transform_system = GetSystem<TransformSystem>())
            {
                transform_system->SubmitTransformUpdates();
            }

            RunRenderUpdatesRange(ExecutionPhase::RenderCollect,
                                  ExecutionPhase::RenderBatch,
                                  deltaTime);

            for (auto& entry : render_system_order)
            {
                if (!entry.system)
                    continue;

                if (entry.phase < static_cast<int>(ExecutionPhase::RenderCollect))
                    continue;

                if (entry.phase > static_cast<int>(ExecutionPhase::RenderStat))
                    continue;

                if (entry.system)
                {
                    HGL_CAPTURE_SCOPE();
                    LogDebug("[ECS] Render Begin: %s", entry.system->GetName().c_str());
                    entry.system->Render(cmd, deltaTime);
                    LogDebug("[ECS] Render End: %s", entry.system->GetName().c_str());
                }
            }

            // Render sub-worlds attached via SubWorldComponent
            if (sub_world_auto_update)
            {
                const auto stats = DispatchLocalRenderSubWorldComponents(
                    this,
                    [cmd, deltaTime](SubWorldComponent& sub_world)
                    {
                        sub_world.RenderSubWorld(cmd, deltaTime);
                    });

#if ULRE_ECS_DEBUG_API
                static bool logged_once = false;
                if (!logged_once && stats.shared_count > 0)
                {
                    logged_once = true;
                    LogDebug("[ECSContext] Render auto sub-world dispatch: shared=%u isolated=%u dispatched=%u",
                             stats.shared_count,
                             stats.isolated_count,
                             stats.dispatched_count);
                }
#endif
            }

            // (Phase 1) 清除当前命令缓冲区（如果是我们设置的）
            if (current_render_cmd == cmd) {
                current_render_cmd = nullptr;
            }
        }

        void ECSContext::RenderDrawOnly(graph::RenderCmdBuffer* cmd, float deltaTime)
        {
            if (!active)
                return;

            // Draw-only entry: Update() phases (Collect/Batch/Upload) were already executed
            // by PrepareRenderPassSetup() or SubWorldComponent::PrepareSubWorld() before
            // BeginRenderPass. Only issue GPU draw commands here.
            if (!current_render_cmd && cmd)
                current_render_cmd = cmd;

            if (auto transform_system = GetSystem<TransformSystem>())
                transform_system->SubmitTransformUpdates();

            for (auto& entry : render_system_order)
            {
                if (!entry.system)
                    continue;

                if (entry.phase < static_cast<int>(ExecutionPhase::RenderCollect))
                    continue;

                if (entry.phase > static_cast<int>(ExecutionPhase::RenderStat))
                    continue;

                HGL_CAPTURE_SCOPE();
                LogDebug("[ECS] RenderDrawOnly: %s", entry.system->GetName().c_str());
                entry.system->Render(cmd, deltaTime);
            }

            // Recurse into nested sub-worlds (draw-only — they were also Prepare'd before the pass)
            if (sub_world_auto_update)
            {
                const auto stats = DispatchLocalRenderSubWorldComponents(
                    this,
                    [cmd, deltaTime](SubWorldComponent& sub_world)
                    {
                        sub_world.DrawSubWorld(cmd, deltaTime);
                    });

#if ULRE_ECS_DEBUG_API
                static bool logged_once = false;
                if (!logged_once && stats.shared_count > 0)
                {
                    logged_once = true;
                    LogDebug("[ECSContext] RenderDrawOnly auto sub-world dispatch: shared=%u isolated=%u dispatched=%u",
                             stats.shared_count,
                             stats.isolated_count,
                             stats.dispatched_count);
                }
#endif
            }

            if (current_render_cmd == cmd)
                current_render_cmd = nullptr;
        }

        void ECSContext::OnResize(const VkExtent2D &extent)
        {
            HGL_CAPTURE_SCOPE();

            if (!active)
                return;

            // Log resize event
                LogInfo("[ECSContext] OnResize: %s %ux%u",
                    GetName().c_str(), extent.width, extent.height);

            // Ensure render target viewport/UBO are updated immediately for this extent.
            if (render_target)
                render_target->OnResize(extent);

            // Notify RenderTargetSystem to sync viewport and dependent systems
            auto render_target_system = GetSystem<RenderTargetSystem>();
            if (render_target_system)
            {
                // RenderTargetSystem will sync CameraSystem viewport
                render_target_system->SetRenderTarget(render_target);
            }
            else
            {
                // Fallback: directly update CameraSystem if no RenderTargetSystem
                auto camera_system = GetSystem<CameraSystem>();
                if (camera_system && render_target)
                    camera_system->SetViewportInfo(render_target->GetViewportInfo());

                // CN: LineRenderSystem 会在 Render 时延迟初始化
                // EN: LineRenderSystem lazy-inits on first Render
            }
        }

        void ECSContext::Render(float deltaTime)
        {
            if (use_adaptive_render_graph)
            {
                // Gather scene statistics and check if we need to rebuild the adaptive graph
                SceneStats stats = GatherSceneStats(this);
                uint64_t current_hash = stats.GetHash();

                if (current_hash != cached_adaptive_scene_hash)
                {
                    LogDebug("[ECS] Adaptive RenderGraph scene hash changed: %llu -> %llu, regenerating",
                             cached_adaptive_scene_hash, current_hash);
                    cached_adaptive_render_graph = CreateAdaptiveRenderGraph(this);
                    cached_adaptive_scene_hash = current_hash;
                }

                Render(deltaTime, cached_adaptive_render_graph);
            }
            else
            {
                // Lazy-initialize default graph cache on first use
                if (!default_render_graph_initialized)
                {
                    cached_default_render_graph = CreateDefaultLinearGraph(this);
                    default_render_graph_initialized = true;
                }
                Render(deltaTime, cached_default_render_graph);
            }
        }

        void ECSContext::Render(float deltaTime, const std::function<void(float)> &pre_render)
        {
            if (use_adaptive_render_graph)
            {
                // Gather scene statistics and check if we need to rebuild the adaptive graph
                SceneStats stats = GatherSceneStats(this);
                uint64_t current_hash = stats.GetHash();

                if (current_hash != cached_adaptive_scene_hash)
                {
                    LogDebug("[ECS] Adaptive RenderGraph scene hash changed: %llu -> %llu, regenerating",
                             cached_adaptive_scene_hash, current_hash);
                    cached_adaptive_render_graph = CreateAdaptiveRenderGraph(this);
                    cached_adaptive_scene_hash = current_hash;
                }

                Render(deltaTime, cached_adaptive_render_graph, pre_render);
            }
            else
            {
                // Lazy-initialize default graph cache on first use
                if (!default_render_graph_initialized)
                {
                    cached_default_render_graph = CreateDefaultLinearGraph(this);
                    default_render_graph_initialized = true;
                }
                Render(deltaTime, cached_default_render_graph, pre_render);
            }
        }

        void ECSContext::RenderPreBeginFrame(float deltaTime)
        {
            if (!active)
                return;

            // Run all phases that execute before the command buffer opens:
            // PreBeginFrame → ResourceSetup → MaterialBind (strict enum order)
            RunRenderPhaseUpdates(ExecutionPhase::RenderPreBeginFrame,  deltaTime);
            RunRenderPhaseUpdates(ExecutionPhase::RenderResourceSetup,  deltaTime);
            RunRenderPhaseUpdates(ExecutionPhase::RenderMaterialBind,   deltaTime);
        }

        void ECSContext::RenderResourceSetup(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderResourceSetup, deltaTime);
        }

        void ECSContext::RenderMaterialBind(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderMaterialBind, deltaTime);
        }

        void ECSContext::RenderSwapchainNextImage(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderSwapchainNextImage, deltaTime);
        }

        bool ECSContext::AcquireSwapchainImage(float deltaTime)
        {
            if (!active)
                return false;

            bool swapchain_ok = true;
            bool swapchain_system_present = false;

            RenderSwapchainNextImage(deltaTime);

            if (auto swapchain_system = GetSystem<SwapchainNextImageSystem>())
            {
                swapchain_system_present = true;
                swapchain_ok = swapchain_system->WasSuccessful();
            }

            if (!swapchain_system_present)
            {
                if (auto* target = GetRenderTarget())
                {
                    if (auto swapchain_rt = dynamic_cast<graph::SwapchainRenderTarget*>(target))
                        swapchain_ok = swapchain_rt->NextFrame();
                }
            }

            return swapchain_ok;
        }

        void ECSContext::SyncRenderTargetViewport()
        {
            if (!active)
                return;

            auto *target = GetRenderTarget();
            if (!target)
                return;

            const VkExtent2D &ext = target->GetExtent();
            const auto *vp_info = target->GetViewportInfo();

            if (vp_info && (vp_info->GetViewport().x != ext.width || vp_info->GetViewport().y != ext.height))
                target->OnResize(ext);
        }

        void ECSContext::RenderBeginFrame(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderBeginFrame, deltaTime);
        }

        void ECSContext::RenderBufferCommit(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderBufferCommit, deltaTime);
        }

        void ECSContext::RenderBufferUpload(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderBufferUpload, deltaTime);
        }

        void ECSContext::RenderFrameSync(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderFrameSync, deltaTime);

        #if ULRE_ECS_DEBUG_API
            if (descriptor_contract_diag_log_enabled)
            {
                using namespace std::chrono;
                const uint64_t now_ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();

                if (descriptor_contract_diag_last_log_ms == 0 || now_ms - descriptor_contract_diag_last_log_ms >= 1000)
                {
                    uint32_t materials_checked = 0;
                    uint32_t materials_unresolved = 0;
                    uint32_t required_missing = 0;
                    uint32_t optional_missing = 0;
                    uint32_t fallback_hits = 0;
                    uint32_t materials_registered = 0;
                    uint32_t binding_entries = 0;

                    if (GetDescriptorContractDiagnosticsExtended(materials_checked,
                                                                 materials_unresolved,
                                                                 required_missing,
                                                                 optional_missing,
                                                                 fallback_hits,
                                                                 materials_registered,
                                                                 binding_entries))
                    {
                        LogInfo("[DescriptorContract][ECSContext] checked=%u unresolved=%u required_missing=%u optional_missing=%u fallback_hits=%u registered_materials=%u registered_bindings=%u",
                                materials_checked,
                                materials_unresolved,
                                required_missing,
                                optional_missing,
                                fallback_hits,
                                materials_registered,
                                binding_entries);

                        std::map<std::string, uint32_t> category_histogram;
                        if (GetShaderGenValidationCategoryHistogram(category_histogram, 128))
                        {
                            const auto count_of = [&category_histogram](const char *category) -> uint32_t
                            {
                                auto it = category_histogram.find(category);
                                return it == category_histogram.end() ? 0u : it->second;
                            };

                            const uint32_t strict_prebuild = count_of("StrictGate.Prebuild");
                            const uint32_t strict_spv = count_of("StrictGate.Spv");
                            const uint32_t strict_vertex = count_of("StrictGate.Vertex");
                            const uint32_t strict_descriptor = count_of("StrictGate.Descriptor");
                            const uint32_t strict_total = strict_prebuild + strict_spv + strict_vertex + strict_descriptor;

                            if (strict_total > 0)
                            {
                                uint32_t strict_materials = 0;
                                std::map<std::string, std::map<std::string, uint32_t>> material_category_matrix;
                                if (GetShaderGenValidationMaterialCategoryMatrix(material_category_matrix, 128))
                                {
                                    for (const auto &mat_pair : material_category_matrix)
                                    {
                                        bool has_strict = false;
                                        for (const auto &cat_pair : mat_pair.second)
                                        {
                                            if (cat_pair.second > 0 && cat_pair.first.rfind("StrictGate.", 0) == 0)
                                            {
                                                has_strict = true;
                                                break;
                                            }
                                        }

                                        if (has_strict)
                                            ++strict_materials;
                                    }
                                }

                                LogInfo("[ShaderGenValidation][ECSContext] strict_total=%u prebuild=%u spv=%u vertex=%u descriptor=%u strict_materials=%u",
                                        strict_total,
                                        strict_prebuild,
                                        strict_spv,
                                        strict_vertex,
                                        strict_descriptor,
                                        strict_materials);
                            }
                        }
                    }

                    descriptor_contract_diag_last_log_ms = now_ms;
                }
            }
        #endif
        }

        void ECSContext::PrepareRenderPassSetup(uint32_t frameIndex, float deltaTime)
        {
            if (!active)
                return;

            // Strict enum order — all CPU work and GPU uploads happen
            // before BeginRenderPass; the render pass only issues draw commands.
            SetFrameIndex(frameIndex);
            RenderBeginFrame(deltaTime);                                         // open cmd buffer, record frame UBOs
            RunRenderPhaseUpdates(ExecutionPhase::RenderCollect,     deltaTime); // collect / cull visible components
            // SubmitTransformUpdates() MUST precede RenderBatch so WriteItems() reads correct
            // world matrices from TransformDataStorage (not uninitialized identity values).
            if (auto transform_system = GetSystem<TransformSystem>())
                transform_system->SubmitTransformUpdates();
            RunRenderPhaseUpdates(ExecutionPhase::RenderBatch,       deltaTime); // write VABs (StagedBuffer → marks dirty)
            RenderBufferCommit(deltaTime);                                       // finalize staged CPU writes
            RenderBufferUpload(deltaTime);                                       // GPU transfer (dirty → uploaded)
            RenderFrameSync(deltaTime);                                          // sync UBOs/descriptors after upload
        }
        void ECSContext::RenderSubmit(float deltaTime)
        {
            if (!active)
                return;

            RunRenderPhaseUpdates(ExecutionPhase::RenderSubmit, deltaTime);
        }

        bool ECSContext::SubmitFrameToRenderTarget(float deltaTime)
        {
            if (!active)
                return false;

            bool submit_ok = false;
            bool submit_system_present = false;

            RenderSubmit(deltaTime);

            if (auto submit_system = GetSystem<SwapchainSubmitSystem>())
            {
                submit_system_present = true;
                submit_ok = submit_system->WasSuccessful();
            }

            if (!submit_system_present)
            {
                if (auto* target = GetRenderTarget())
                    submit_ok = target->Submit();
            }

            return submit_ok;
        }

        void ECSContext::RunRenderPhaseUpdates(ExecutionPhase phase, float deltaTime)
        {
            SortRenderSystems();

            for (auto& entry : render_system_order)
            {
                if (!entry.system)
                    continue;

                if (entry.phase != static_cast<int>(phase))
                    continue;

                RunSystemUpdate(entry.system.get(), deltaTime);
            }
        }

        void ECSContext::RunRenderUpdatesFrom(ExecutionPhase phase, float deltaTime)
        {
            SortRenderSystems();

            const int min_phase = static_cast<int>(phase);

            for (auto& entry : render_system_order)
            {
                if (!entry.system)
                    continue;

                if (entry.phase < min_phase)
                    continue;

                RunSystemUpdate(entry.system.get(), deltaTime);
            }
        }

        void ECSContext::RunRenderUpdatesRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime)
        {
            SortRenderSystems();

            const int min_phase = static_cast<int>(minPhase);
            const int max_phase = static_cast<int>(maxPhase);

            for (auto& entry : render_system_order)
            {
                if (!entry.system)
                    continue;

                if (entry.phase < min_phase || entry.phase > max_phase)
                    continue;

                RunSystemUpdate(entry.system.get(), deltaTime);
            }
        }

        void ECSContext::RunSystemUpdate(System *system, float deltaTime)
        {
            if (!system)
                return;
            if (!system->IsEnabled())
                return;

            HGL_CAPTURE_SCOPE();
            LogDebug("[ECS] Update Begin: %s", system->GetName().c_str());

            if (system_profiling_enabled)
                profiler.Begin(system);
            system->Update(deltaTime);
            if (system_profiling_enabled)
                profiler.End(system);

            LogDebug("[ECS] Update End: %s", system->GetName().c_str());
        }

        void ECSContext::RunRenderSystemsInRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime)
        {
            SortRenderSystems();

            if (gpu_device)
                gpu_device->SetDrawPhaseActive(true);

            const int min_phase = static_cast<int>(minPhase);
            const int max_phase = static_cast<int>(maxPhase);

            for (auto& entry : render_system_order)
            {
                if (!entry.system || !entry.system->IsEnabled())
                    continue;

                if (entry.phase < min_phase || entry.phase > max_phase)
                    continue;

                HGL_CAPTURE_SCOPE();
                LogDebug("[ECS] Render Begin: %s (phase %d)", entry.system->GetName().c_str(), entry.phase);
                
                if (system_profiling_enabled)
                    profiler.Begin(entry.system.get());
                entry.system->Render(current_render_cmd, deltaTime);
                if (system_profiling_enabled)
                    profiler.End(entry.system.get());
                
                LogDebug("[ECS] Render End: %s", entry.system->GetName().c_str());
            }

            if (gpu_device)
                gpu_device->SetDrawPhaseActive(false);
        }

        void ECSContext::ClearEntities()
        {
            if (entity_manager)
                entity_manager->Clear();

            component_registry.Clear();
            static_transforms.clear();
            movable_transforms.clear();
        }

        void ECSContext::SortTickSystems()
        {
            SortSystemList(tick_system_order, tick_dependencies, tick_order_dirty, "Tick");
        }

        void ECSContext::SortRenderSystems()
        {
            SortSystemList(render_system_order, render_dependencies, render_order_dirty, "Render");
        }

        void ECSContext::SortSystemList(std::vector<OrderedSystem>& order_list,
                                         const DependencyMap& dependencies,
                                         bool& dirty_flag,
                                         const char* label)
        {
            if (!dirty_flag)
                return;

            if (order_list.empty())
            {
                dirty_flag = false;
                return;
            }

            hgl::UnorderedMap<size_t, size_t> index_map;
            index_map.Reserve(order_list.size());

            for (size_t i = 0; i < order_list.size(); ++i)
            {
                index_map[order_list[i].key] = i;
            }

            std::vector<size_t> indegree(order_list.size(), 0);
            std::vector<std::vector<size_t>> adj(order_list.size());

            for (const auto& pair : dependencies)
            {
                const size_t dependent_key = pair.first;
                const size_t* dependent_index = index_map.GetValuePointer(dependent_key);
                if (!dependent_index)
                    continue;

                for (const size_t dependency_key : pair.second)
                {
                    const size_t* dependency_index = index_map.GetValuePointer(dependency_key);
                    if (!dependency_index)
                    {
                    #ifdef _DEBUG
                        LogWarning("[ECSContext::SortSystemList] %s dependency missing for key %zu -> %zu",
                                   label,
                                   dependent_key,
                                   dependency_key);
                    #endif
                        continue;
                    }

                    adj[*dependency_index].push_back(*dependent_index);
                    ++indegree[*dependent_index];
                }
            }

            auto order_compare = [&order_list](size_t a, size_t b)
            {
                const auto& lhs = order_list[a];
                const auto& rhs = order_list[b];
                // First sort by phase
                if (lhs.phase != rhs.phase)
                    return lhs.phase < rhs.phase;
                // Then use insertion order for stable sort
                return lhs.insertion_order < rhs.insertion_order;
            };

            std::vector<size_t> available;
            available.reserve(order_list.size());

            for (size_t i = 0; i < order_list.size(); ++i)
            {
                if (indegree[i] == 0)
                    available.push_back(i);
            }

            std::vector<OrderedSystem> sorted;
            sorted.reserve(order_list.size());

            while (!available.empty())
            {
                std::sort(available.begin(), available.end(), order_compare);
                const size_t current = available.front();
                available.erase(available.begin());

                sorted.push_back(order_list[current]);

                for (const size_t next : adj[current])
                {
                    if (--indegree[next] == 0)
                        available.push_back(next);
                }
            }

            if (sorted.size() != order_list.size())
            {
                std::stable_sort(order_list.begin(), order_list.end(),
                    [](const OrderedSystem& a, const OrderedSystem& b)
                    {
                        if (a.phase != b.phase)
                            return a.phase < b.phase;
                        return a.insertion_order < b.insertion_order;
                    });

                LogWarning("[ECSContext::SortSystemList] %s system dependencies contain a cycle. Falling back to phase/insertion order.",
                           label);
            }
            else
            {
                order_list.swap(sorted);
            }

            dirty_flag = false;
        }

        ECSContext::OrderedSystem* ECSContext::FindOrderedSystem(std::vector<OrderedSystem>& list, size_t key)
        {
            for (auto& entry : list)
            {
                if (entry.key == key)
                    return &entry;
            }

            return nullptr;
        }

        void ECSContext::AddOrUpdateSystem(bool is_render, size_t key, const std::shared_ptr<System>& system)
        {
            if (!system)
                return;

            // Set the context for the system so it can access entities and create queries
            system->SetContext(this);

            const int effective_phase = static_cast<int>(system->GetExecutionPhase());
            const bool phase_is_render = effective_phase >= static_cast<int>(ExecutionPhase::RenderSwapchainNextImage);
            const bool effective_is_render = phase_is_render;

            if (effective_is_render != is_render)
            {
                LogWarning("[ECS] System '%s' registration corrected by phase (%s -> %s)",
                           system->GetName().c_str(),
                           is_render ? "render" : "tick",
                           effective_is_render ? "render" : "tick");
            }

            if (effective_is_render && !allow_render_system_registration)
            {
                ++rejected_render_system_registration_count;

            #if ULRE_ECS_DEBUG_API
                if (!rejected_render_system_registration_logged)
                {
                    rejected_render_system_registration_logged = true;
                    LogWarning("[ECS] Render system registration rejected in context '%s'. Example system='%s'",
                               GetName().c_str(),
                               system->GetName().c_str());
                }
            #endif
                return;
            }

            auto& sys_map = effective_is_render ? render_systems : tick_systems;
            auto& order_list = effective_is_render ? render_system_order : tick_system_order;
            auto& dirty_flag = effective_is_render ? render_order_dirty : tick_order_dirty;
            auto& deps = effective_is_render ? render_dependencies : tick_dependencies;
            const bool is_new_registration = (sys_map.GetValuePointer(key) == nullptr);

            if (is_new_registration)
            {
                if (effective_is_render && context_role == ContextRole::RootShared)
                    ++global_render_system_count;

                if (!effective_is_render && context_role == ContextRole::LocalSubWorld)
                    ++local_gameplay_system_count;
            }

            sys_map[key] = system;

            // Register in element type map
            const std::string& element_type = system->GetRenderElementType();
            if (!element_type.empty())
            {
                auto& systems_list = systems_by_element_type[element_type];
                // Remove if already exists (in case of update)
                for (auto it = systems_list.begin(); it != systems_list.end(); ++it)
                {
                    if (*it == system)
                    {
                        systems_list.erase(it);
                        break;
                    }
                }
                // Add to list
                systems_list.push_back(system);
            }

            if (auto *entry = FindOrderedSystem(order_list, key))
            {
                entry->system = system;
                entry->phase = effective_phase;
                dirty_flag = true;
            }
            else
            {
                OrderedSystem new_entry;
                new_entry.key = key;
                new_entry.phase = effective_phase;
                new_entry.insertion_order = next_system_order++;
                new_entry.system = system;
                order_list.push_back(std::move(new_entry));
                dirty_flag = true;
            }

            deps.DeleteByKey(key);

            // Automatically register dependencies declared by the system
            const auto& deps_decl = system->GetDependencies();
            for (const auto& dep_type : deps_decl)
            {
                size_t dep_key = dep_type.hash_code();
                AddSystemDependency(effective_is_render, key, dep_key);
            }
            if (active)
            {
                system->OnDependenciesReady();
                system->Initialize();
            }
        }

        void ECSContext::AddSystemDependency(bool is_render, size_t dependent_key, size_t dependency_key)
        {
            if (dependent_key == dependency_key)
                return;

            auto& deps = is_render ? render_dependencies : tick_dependencies;
            auto& order_dirty = is_render ? render_order_dirty : tick_order_dirty;

            auto* list = deps.GetValuePointer(dependent_key);
            if (!list)
            {
                deps.Add(dependent_key, std::vector<size_t>{});
                list = deps.GetValuePointer(dependent_key);
            }

            const auto it = std::find(list->begin(), list->end(), dependency_key);
            if (it == list->end())
            {
                list->push_back(dependency_key);
                order_dirty = true;
            }
        }
        bool ECSContext::SetSystemEnabledByKey(size_t key, bool enabled)
        {
            if (auto *system = tick_systems.GetValuePointer(key))
            {
                if (*system)
                    (*system)->SetEnabled(enabled);
                return true;
            }

            if (auto *system = render_systems.GetValuePointer(key))
            {
                if (*system)
                    (*system)->SetEnabled(enabled);
                return true;
            }

            return false;
        }

        bool ECSContext::RemoveSystemByKey(size_t key)
        {
            auto remove_from = [&](auto &sys_map,
                                   auto &order_list,
                                   auto &deps,
                                   bool &order_dirty) -> bool
            {
                auto *holder = sys_map.GetValuePointer(key);
                if (!holder)
                    return false;

                const auto removed_system = *holder;
                const int removed_phase = removed_system ? static_cast<int>(removed_system->GetExecutionPhase()) : -1;

                if (*holder)
                    (*holder)->Shutdown();

                sys_map.DeleteByKey(key);
                deps.DeleteByKey(key);

                for (auto &pair : deps)
                {
                    auto &vec = pair.second;
                    vec.erase(std::remove(vec.begin(), vec.end(), key), vec.end());
                }

                order_list.erase(std::remove_if(order_list.begin(),
                                                order_list.end(),
                                                [key](const OrderedSystem &entry)
                                                {
                                                    return entry.key == key;
                                                }),
                                 order_list.end());

                if (removed_system)
                {
                    const bool removed_is_render = removed_phase >= static_cast<int>(ExecutionPhase::RenderSwapchainNextImage);

                    if (removed_is_render && context_role == ContextRole::RootShared && global_render_system_count > 0)
                        --global_render_system_count;

                    if (!removed_is_render && context_role == ContextRole::LocalSubWorld && local_gameplay_system_count > 0)
                        --local_gameplay_system_count;

                    for (auto it = systems_by_element_type.begin(); it != systems_by_element_type.end();)
                    {
                        auto& vec = it->second;
                        vec.erase(std::remove(vec.begin(), vec.end(), removed_system), vec.end());

                        if (vec.empty())
                            it = systems_by_element_type.erase(it);
                        else
                            ++it;
                    }
                }

                order_dirty = true;
                return true;
            };

            if (remove_from(tick_systems, tick_system_order, tick_dependencies, tick_order_dirty))
                return true;

            if (remove_from(render_systems, render_system_order, render_dependencies, render_order_dirty))
                return true;

            return false;
        }

        void ECSContext::SetFrameIndex(const uint32_t index)
        {
            frame_index = index;
            TransformAssignmentBuffer::SetFrameIndex(index);
        }

        void ECSContext::RegisterComponentInstance(size_t type_hash, const std::shared_ptr<Component>& comp)
        {
            if(!comp)
                return;

            RegisterComponentInstanceInternal(type_hash, comp);

            const char* group_name = comp->GetSystemGroupName();
            if (group_name && group_name[0] != '\0')
            {
                const std::string group(group_name);
                known_system_groups.insert(group);

                auto& count = system_group_component_counts[group];
                ++count;

                EnsureSystemGroupSystems(this, group, GetRenderTarget());
                SetElementTypeSystemsEnabled(group, true);

                profiler.MarkGroupEnsured(group);
                profiler.UpdateGroupState(group, count, true);
            }

            for (const auto& entry : component_query_bases)
            {
                if (entry.key == type_hash)
                    continue;

                if (entry.matches && entry.matches(comp.get()))
                    RegisterComponentInstanceInternal(entry.key, comp);
            }
        }

        void ECSContext::UnregisterComponentInstance(size_t type_hash, Component* comp_ptr)
        {
            (void)type_hash;
            if (!comp_ptr)
                return;

            const char* group_name = comp_ptr->GetSystemGroupName();
            if (group_name && group_name[0] != '\0')
            {
                const std::string group(group_name);
                known_system_groups.insert(group);

                auto it = system_group_component_counts.find(group);
                if (it != system_group_component_counts.end())
                {
                    if (it->second > 0)
                        --(it->second);

                    if (it->second == 0)
                    {
                        SetElementTypeSystemsEnabled(group, false);
                    }

                    profiler.UpdateGroupState(group, it->second, it->second > 0);
                }
            }

            for (auto& pair : component_registry)
            {
                auto& vec = pair.second;
                vec.erase(std::remove_if(vec.begin(), vec.end(), [comp_ptr](const std::weak_ptr<Component>& w){
                    auto sp = w.lock();
                    return !sp || sp.get()==comp_ptr;
                }), vec.end());
            }
        }

        void ECSContext::RegisterComponentInstanceInternal(size_t type_hash, const std::shared_ptr<Component>& comp)
        {
            if (!comp)
                return;

            auto* vec = component_registry.GetValuePointer(type_hash);
            if (!vec)
            {
                component_registry.Add(type_hash, std::vector<std::weak_ptr<Component>>{});
                vec = component_registry.GetValuePointer(type_hash);
            }

            for (const auto& weak_comp : *vec)
            {
                if (auto existing = weak_comp.lock())
                {
                    if (existing.get() == comp.get())
                        return;
                }
            }

            vec->push_back(comp);
        }

        void ECSContext::RegisterTransformComponent(const std::shared_ptr<TransformComponent>& comp, bool isMovable)
        {
            if (!comp)
                return;

            // Auto-ensure TransformSystem when any TransformComponent appears.
            // This guarantees transform update/upload path without requiring apps to
            // manually register the system.
            {
                auto transform_system = GetSystem<TransformSystem>();
                if (!transform_system)
                    transform_system = RegisterTickSystem<TransformSystem>();

                if (transform_system)
                {
                    transform_system->SetWorld(this);
                    transform_system->SetEnabled(true);
                }
            }

            if (isMovable)
            {
                movable_transforms.push_back(comp);
            }
            else
            {
                static_transforms.push_back(comp);
            }
        }

        void ECSContext::MigrateTransformComponent(TransformComponent* comp_ptr, bool toMovable)
        {
            if (!comp_ptr)
                return;

            auto remove_from_list = [comp_ptr](std::vector<std::weak_ptr<TransformComponent>>& list)
            {
                list.erase(std::remove_if(list.begin(), list.end(),
                    [comp_ptr](const std::weak_ptr<TransformComponent>& w)
                    {
                        auto sp = w.lock();
                        return !sp || sp.get() == comp_ptr;
                    }), list.end());
            };

            // Remove from current list
            if (toMovable)
            {
                remove_from_list(static_transforms);
            }
            else
            {
                remove_from_list(movable_transforms);
            }

            // Add to new list
            std::shared_ptr<TransformComponent> comp_shared;

            if (auto owner = comp_ptr->GetOwner())
            {
                comp_shared = owner->GetComponent<TransformComponent>();
            }

            if (!comp_shared)
                return;

            auto add_unique = [&comp_ptr](std::vector<std::weak_ptr<TransformComponent>>& list,
                                          const std::shared_ptr<TransformComponent>& comp)
            {
                for (const auto& weak_comp : list)
                {
                    if (auto existing = weak_comp.lock())
                    {
                        if (existing.get() == comp_ptr)
                            return;
                    }
                }
                list.push_back(comp);
            };

            if (toMovable)
                add_unique(movable_transforms, comp_shared);
            else
                add_unique(static_transforms, comp_shared);
        }

        void ECSContext::UnregisterTransformComponent(TransformComponent* comp_ptr)
        {
            if (!comp_ptr)
                return;

            auto remove_from_list = [comp_ptr](std::vector<std::weak_ptr<TransformComponent>>& list)
            {
                list.erase(std::remove_if(list.begin(), list.end(),
                    [comp_ptr](const std::weak_ptr<TransformComponent>& w)
                    {
                        auto sp = w.lock();
                        return !sp || sp.get() == comp_ptr;
                    }), list.end());
            };

            remove_from_list(static_transforms);
            remove_from_list(movable_transforms);
        }

        void ECSContext::NotifyComponentAdded(EntityID entity_id, const std::type_index& component_type)
        {
            // Get entity for predicate checking
            Entity* entity = GetEntity(entity_id);
            if (!entity)
                return;

            // Notify all tick systems - directly push entity to matching queries
            for (auto& [key, system] : tick_systems)
            {
                if (system && system->GetCache())
                {
                    system->GetCache()->OnComponentAdded(entity_id, component_type, entity);
                }
            }

            // Notify all render systems - directly push entity to matching queries
            for (auto& [key, system] : render_systems)
            {
                if (system && system->GetCache())
                {
                    system->GetCache()->OnComponentAdded(entity_id, component_type, entity);
                }
            }
        }

        void ECSContext::NotifyComponentRemoved(EntityID entity_id, const std::type_index& component_type)
        {
            // Notify all tick systems - remove entity from affected queries
            for (auto& [key, system] : tick_systems)
            {
                if (system && system->GetCache())
                {
                    system->GetCache()->OnComponentRemoved(entity_id, component_type);
                }
            }

            // Notify all render systems - remove entity from affected queries
            for (auto& [key, system] : render_systems)
            {
                if (system && system->GetCache())
                {
                    system->GetCache()->OnComponentRemoved(entity_id, component_type);
                }
            }
        }

        void ECSContext::NotifyEntityDestroyed(EntityID entity_id)
        {
            // Notify all tick systems - remove entity from all queries
            for (auto& [key, system] : tick_systems)
            {
                if (system && system->GetCache())
                {
                    system->GetCache()->OnEntityDestroyed(entity_id);
                    const size_t removed = system->GetCache()->RemoveInvalidEntities();
                    if (removed > 0)
                    {
                        LogWarning("[ECS] Pruned %zu stale query entries in tick system %s", removed, system->GetName().c_str());
                    }
                }
            }

            // Notify all render systems - remove entity from all queries
            for (auto& [key, system] : render_systems)
            {
                if (system && system->GetCache())
                {
                    system->GetCache()->OnEntityDestroyed(entity_id);
                    const size_t removed = system->GetCache()->RemoveInvalidEntities();
                    if (removed > 0)
                    {
                        LogWarning("[ECS] Pruned %zu stale query entries in render system %s", removed, system->GetName().c_str());
                    }
                }
            }
        }

        void ECSContext::GetSystemsByElementType(const std::string& element_type, std::vector<std::shared_ptr<System>>& out_systems) const
        {
            out_systems.clear();
            auto it = systems_by_element_type.find(element_type);
            if (it != systems_by_element_type.end())
            {
                out_systems = it->second;
            }
        }

        void ECSContext::GetAllRenderElementTypes(std::vector<std::string>& out_element_types) const
        {
            out_element_types.clear();
            out_element_types.reserve(systems_by_element_type.size());

            for (const auto& pair : systems_by_element_type)
            {
                out_element_types.push_back(pair.first);
            }
        }

        void ECSContext::SetElementTypeSystemsEnabled(const std::string& element_type, bool enabled)
        {
            auto it = systems_by_element_type.find(element_type);
            if (it != systems_by_element_type.end())
            {
                for (auto& system : it->second)
                {
                    if (system)
                    {
                        system->SetEnabled(enabled);
                    }
                }
            }
        }

        uint32_t ECSContext::GetSystemGroupComponentCount(const std::string& group_name) const
        {
            auto it = system_group_component_counts.find(group_name);
            if (it == system_group_component_counts.end())
                return 0;

            return it->second;
        }

        bool ECSContext::IsSystemGroupInstalled(const std::string& group_name) const
        {
            if (group_name.empty())
                return false;

            return installed_system_groups.find(group_name) != installed_system_groups.end();
        }

        void ECSContext::MarkSystemGroupInstalled(const std::string& group_name)
        {
            if (group_name.empty())
                return;

            installed_system_groups.insert(group_name);
        }

        void ECSContext::GetKnownSystemGroups(std::vector<std::string>& out_group_names) const
        {
            out_group_names.clear();
            out_group_names.reserve(known_system_groups.size());

            for (const auto& group_name : known_system_groups)
                out_group_names.push_back(group_name);
        }

        void ECSContext::DisableUnusedSystemGroups()
        {
            for (const auto& group_name : known_system_groups)
            {
                if (GetSystemGroupComponentCount(group_name) == 0)
                {
                    SetElementTypeSystemsEnabled(group_name, false);
                    profiler.UpdateGroupState(group_name, 0, false);
                }
            }
        }

        bool ECSContext::DisableSystemGroup(const std::string& group_name)
        {
            if (group_name.empty())
                return false;

            SetElementTypeSystemsEnabled(group_name, false);
            profiler.UpdateGroupState(group_name, GetSystemGroupComponentCount(group_name), false);
            return true;
        }

        bool ECSContext::CleanupSystemGroup(const std::string& group_name, bool remove_systems)
        {
            if (group_name.empty())
                return false;

            SetElementTypeSystemsEnabled(group_name, false);
            profiler.UpdateGroupState(group_name, GetSystemGroupComponentCount(group_name), false);

            if (!remove_systems)
                return true;

            auto it = systems_by_element_type.find(group_name);
            if (it == systems_by_element_type.end())
                return false;

            std::vector<std::shared_ptr<System>> systems = it->second;
            for (const auto& system : systems)
            {
                if (!system)
                    continue;

                const size_t key = typeid(*system).hash_code();
                RemoveSystemByKey(key);
            }

            systems_by_element_type.erase(group_name);
            profiler.RemoveGroupProfile(group_name);
            return true;
        }

        size_t ECSContext::CleanupUnusedSystemGroups(bool remove_systems)
        {
            std::vector<std::string> targets;
            targets.reserve(known_system_groups.size());

            for (const auto& group_name : known_system_groups)
            {
                if (GetSystemGroupComponentCount(group_name) == 0)
                    targets.push_back(group_name);
            }

            size_t cleaned = 0;
            for (const auto& group_name : targets)
            {
                if (CleanupSystemGroup(group_name, remove_systems))
                    ++cleaned;
            }

            return cleaned;
        }
    }//namespace ecs
}//namespace hgl


