#include<hgl/ecs/core/Context.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/ecs/core/EntityManager.h>
#include<hgl/ecs/core/DefaultSystems.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/VisibilitySystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/MaterialComponent.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/support/RenderPipelineBase.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/ecs/support/RenderItemDataStorage.h>
#include<hgl/ecs/support/DrawItemIDStorage.h>
#include<hgl/ecs/systems/render/RenderSystemCore.h>
#include<hgl/ecs/systems/render/RenderTargetSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/systems/render/SwapchainNextImageSystem.h>
#include<hgl/ecs/systems/render/SwapchainSubmitSystem.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GraphModuleManager.h>
#include<hgl/graph/module/SwapchainModule.h>
#include<hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/log/Log.h>
#include<hgl/object/ObjectTracker.h>
#include<algorithm>
#include<chrono>
#include<cstdio>
#include<vector>

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
            , transform_storage(std::make_unique<TransformDataStorage>())
            , render_item_storage(std::make_unique<RenderItemDataStorage>())
            , draw_item_id_storage(std::make_unique<DrawItemIDStorage>())
            , active(false)
        {
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

            // Propagate device to RenderBufferUploadSystem if it was registered first
            if (auto upload_system = GetSystem<RenderBufferUploadSystem>())
                upload_system->SetDevice(gpu_device);

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
                    upload_system->SetDevice(gpu_device);
                }
            }

                SortTickSystems();
                SortRenderSystems();

            // Initialize all systems
                for (auto& entry : tick_system_order)
                {
                    if (entry.system)
                    {
                        entry.system->Initialize();
                    }
                }

                for (auto& entry : render_system_order)
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
                camera_system = RegisterTickSystem<CameraSystem>(this);
            }

            if (!camera_system)
                return nullptr;

            if (active)
            {
                camera_system->Initialize();
            }

            return camera_system;
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

//            LogDebug("[ECS] Registering render pipeline: %s", name.c_str());
            render_pipelines[name] = std::move(pipeline);
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
                installed_system_groups.clear();
                static_transforms.clear();
                movable_transforms.clear();

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

            // Destroy all entities
            if (entity_manager)
            {
                entity_manager->Clear();
            }

            component_registry.Clear();
            system_group_component_counts.clear();
            installed_system_groups.clear();
            static_transforms.clear();
            movable_transforms.clear();

            // Finally, clear materialBatches after all systems/entities are destroyed
            LogDebug("[ECSContext] Shutdown - releasing %zu material batches",
                     render_frame_cache.materialBatches.GetCount());
            render_frame_cache.materialBatches.Clear();
            LogDebug("[ECSContext] Shutdown - material batches cleared");
            shutdown_in_progress = false;
        }

        void ECSContext::Tick(float deltaTime)
        {
            if (!active)
                return;

            SortTickSystems();

            // Update non-render systems
            for (auto& entry : tick_system_order)
            {
                if (entry.system)
                    RunSystemUpdate(entry.system.get(), deltaTime);
            }

            // 原每帧全实体 × 全组件的 OnUpdate 虚分发已删除——其唯一有效实现
            // （TransformComponent 的 fixed_pixel 等像素缩放）由 gizmo 的
            // TransformGizmoSystem 在 TickPostCamera 相位经
            // SetFixedPixelSizingContext 即时重算（本就是主通道）。
            // 逻辑更新归 tick 系统，不归组件。

            if (auto input_system = GetSystem<InputSystem>())
            {
                input_system->EndFrame();
            }
        }

        bool ECSContext::EnsureRenderCoreInitialized()
        {
            if (render_core)
                return true;

            render_core = std::make_unique<RenderSystemCore>(this);
            if (!render_core->Initialize())
            {
                render_core.reset();
                return false;
            }

            return true;
        }

        void ECSContext::ExecuteScenePrePassWorkflow(float deltaTime)
        {
            switch (scene_pipeline_mode)
            {
                case ScenePipelineMode::StandardLitCSM:
                {
                    // ── 黄金路径：标准 3D / FPS / TPS 陆地场景主光级联阴影自动化 ──
                    if (auto env = GetSystem<EnvironmentSystem>())
                    {
                        if (env->IsMainLightShadowEnabled())
                        {
                            if (auto cam_sys = GetSystem<CameraSystem>())
                            {
                                if (auto *main_cam = cam_sys->GetMainCameraComponent())
                                {
                                    env->RenderMainLightShadowPass(main_cam, deltaTime);
                                }
                            }
                        }
                    }
                    break;
                }

                case ScenePipelineMode::TopDownRTS:
                case ScenePipelineMode::AerialLowAltitude:
                case ScenePipelineMode::AerialHighAltitude:
                case ScenePipelineMode::Space3D:
                case ScenePipelineMode::SideScroll2D:
                    // 预留特定场景类型硬编码路径空壳
                    break;

                case ScenePipelineMode::Custom:
                default:
                    break;
            }
        }

        bool ECSContext::BeginManagedRenderFrame(float deltaTime, const bool need_swapchain_acquire, const graph::RenderPassOptions *options)
        {
            if (!active)
                return false;

            if (!EnsureRenderCoreInitialized())
                return false;

            if (GetRenderTarget() && need_swapchain_acquire)
            {
//                LogInfo("[ECS RENDER] Calling AcquireSwapchainImage");
                if (!AcquireSwapchainImage(deltaTime))
                {
                    LogWarning("[ECS RENDER] AcquireSwapchainImage FAILED");
                    return false;
                }
            }

//            LogInfo("[ECS RENDER] Calling RenderPreBeginFrame");
            RenderPreBeginFrame(deltaTime);

            if (need_swapchain_acquire)
            {
                // 主世界渲染帧前置阶段：按当前场景工作流硬编码执行预处理 Pass（如 CSM 级联阴影）
                ExecuteScenePrePassWorkflow(deltaTime);
            }

            SyncRenderTargetViewport();

            if (auto *gc = GetGraphicsContext())
            {
                if (auto *tm = gc->GetTextureManager())
                {
                    tm->UpdateUploadQueue(gc->GetBindlessTextureManager());
                }
            }

//            LogInfo("[ECS RENDER] Calling BeginFrame");
            if (!render_core->BeginFrame())
            {
                LogWarning("[ECS RENDER] BeginFrame FAILED");
                return false;
            }

            SetCurrentRenderCmd(render_core->GetRenderCmd());
            PrepareRenderPassSetup(render_core->GetSwapchainImageIndex(), deltaTime);

//            LogInfo("[ECS RENDER] Calling BeginRenderPass");
            if (!render_core->BeginRenderPass(options))
            {
                LogWarning("[ECS RENDER] BeginRenderPass FAILED");
                render_core->EndFrame();
                SetCurrentRenderCmd(nullptr);
                return false;
            }

            return true;
        }

        void ECSContext::EndManagedRenderFrame(float deltaTime)
        {
            if (!render_core)
                return;

//            LogInfo("[ECS RENDER] Calling EndFrame");
            render_core->EndFrame();

            SetCurrentRenderCmd(nullptr);

//            LogInfo("[ECS RENDER] Calling SubmitFrameToRenderTarget");
            if (!SubmitFrameToRenderTarget(deltaTime))
            {
                LogError("[ECS RENDER] SubmitFrameToRenderTarget FAILED");
            }
        }

        void ECSContext::RecordPreparedRenderPhaseRange(ExecutionPhase minPhase,
                                                        ExecutionPhase maxPhase,
                                                        float deltaTime,
                                                        bool submit_transforms,
                                                        const char *log_prefix)
        {
            if (submit_transforms)
            {
                if (auto transform_system = GetSystem<TransformSystem>())
                    transform_system->SubmitTransformUpdates();
            }

            //if (log_prefix)
            //{
            //    LogDebug("%s phase range %d to %d",
            //             log_prefix,
            //             static_cast<int>(minPhase),
            //             static_cast<int>(maxPhase));
            //}

            RunRenderSystemsInRange(minPhase, maxPhase, deltaTime);
        }



        void ECSContext::RenderDrawOnly(graph::RenderCmdBuffer* cmd, float deltaTime)
        {
            if (!active)
                return;

            // Draw-only entry: Update() phases (Collect/Batch/Upload) were already executed
            // by PrepareRenderPassSetup() before BeginRenderPass. Only issue GPU draw commands here.
            if (!current_render_cmd && cmd)
                current_render_cmd = cmd;

            RecordPreparedRenderPhaseRange(ExecutionPhase::RenderCollect,
                                           ExecutionPhase::RenderStat,
                                           deltaTime,
                                           true,
                                           "[ECSContext::RenderDrawOnly]");

            if (current_render_cmd == cmd)
                current_render_cmd = nullptr;
        }

        bool ECSContext::RenderTo(const RenderPassRequest &req)
        {
            if (!active)
                return false;

            graph::IRenderTarget *rt = req.target;
            if (!rt)
            {
                LogError("[ECSContext::RenderTo] render target is null");
                return false;
            }

            // 调用期间把本世界的渲染目标切到目标 RT；结束后恢复。
            // RenderSystemCore::BeginFrame() 每次重取 world->GetRenderTarget()，
            // 因此能正确拿到临时切换后的 RT。
            graph::IRenderTarget *saved_target = render_target;

            // 共享 per-frame 资源保护（上沿）：Camera UBO 与 L2W ring 为单份内存，
            // 覆写前等在途主帧 GPU 完成，防止其 draw 读到覆盖后的数据
            if (saved_target)
                saved_target->WaitFence();

            // clear 覆盖语义：临时改写目标 RT 上的清屏色（唯一权威），
            // 渲染结束（含失败路径）后恢复原声明值。
            const hgl::Color4f saved_clear = rt->GetClearColor();
            if (!req.use_target_clear)
                rt->SetClearColor(req.clear);

            // 必须同步 RenderTargetSystem：它缓存的 RT 若不跟随切换，本 Pass 内
            // CameraSystem 的 viewport 等仍按主 RT 工作，且渲染期管线解析会按
            // 主 RenderPass 键控，把主管线（带颜色附件）画进 depth-only 离屏
            // Pass（VUID-06179/08914，历史上 RenderContext 副本不同步时踩过）。
            auto rts = GetSystem<RenderTargetSystem>();
            graph::IRenderTarget *rts_saved = rts ? rts->GetRenderTarget() : nullptr;
            if (rts)
                rts->SetRenderTarget(rt);

            render_target = rt;

            // pass 级相机覆盖：本 pass 用指定相机解算共享相机数据。
            // SetOverrideCamera 只设覆盖目标，**显式驱动一次 Update**：
            // 覆盖分支只解算覆盖相机（不吃输入）；此时 RTSystem 已把 viewport 同步为本 RT。
            auto camera_system = GetSystem<CameraSystem>();
            const bool use_camera_override = (req.camera != nullptr && camera_system);
            if (use_camera_override)
            {
                camera_system->SetOverrideCamera(req.camera);
                camera_system->Update(req.delta_time);
                active_camera_id = req.camera->camera_id;
            }
            else
            {
                active_camera_id = 0;
            }

            active_mobility_filter = req.mobility_filter;
            is_current_pass_shadow = req.is_shadow_pass;
            has_shadow_origin = false;

            if (req.is_shadow_pass)
            {
                if (req.shadow_reference_camera)
                {
                    current_pass_shadow_origin = glm::vec3(req.shadow_reference_camera->position);
                    has_shadow_origin = true;
                }
                else if (camera_system)
                {
                    auto *cam = camera_system->GetCamera();
                    if (cam)
                    {
                        current_pass_shadow_origin = glm::vec3(cam->pos.x, cam->pos.y, cam->pos.z);
                        has_shadow_origin = true;
                    }
                }
            }

            graph::RenderPassOptions pass_options;
            pass_options.load_depth = req.load_depth;
            pass_options.use_scissor = req.use_scissor;
            pass_options.scissor = req.scissor;
            pass_options.clear_scissor_depth = req.clear_scissor_depth;
            pass_options.clear_depth_value = 0.0f; // Reversed-Z (0.0f = far)
            pass_options.cull_mode = static_cast<int>(req.cull_mode);

            const graph::RenderPassOptions *p_options =
                (req.load_depth || req.use_scissor || req.clear_scissor_depth
                 || req.cull_mode != CullMode::Inherit) ? &pass_options : nullptr;

            bool ok = false;

            // 离屏 RT 无 swapchain 图像可获取，跳过 AcquireSwapchainImage
            if (BeginManagedRenderFrame(req.delta_time, false, p_options))
            {
                RenderDrawOnly(render_core->GetRenderCmd(), req.delta_time);
                EndManagedRenderFrame(req.delta_time);
                ok = true;
            }

            active_mobility_filter = -1;
            is_current_pass_shadow = false;
            has_shadow_origin = false;

            // 关键同步（下沿保护）：本帧离屏提交完成后立即等该 RT 自己的 queue fence！
            // 必须在恢复主相机共享数据前等，因为 GPU 仍在异步读取本 pass 提交的光源
            // CameraUBO；若未等即在 CPU 上 Update(0.0f) 覆盖主相机数据，会踩踏 GPU 正在读取的内存，
            // 导致 shadow map 被画成主相机视角（间歇性阴影闪烁丢失）。
            if (ok)
                rt->WaitFence();

            render_target = saved_target;
            if (!req.use_target_clear)
                rt->SetClearColor(saved_clear);

            if (rts)
                rts->SetRenderTarget(rts_saved ? rts_saved : saved_target);

            if (use_camera_override)
            {
                camera_system->RestoreMainCamera();
            }
            active_camera_id = 0;

            return ok;
        }

        bool ECSContext::RenderTo(graph::IRenderTarget *rt, const hgl::Color4f &clear, float deltaTime, CullMode cull_mode)
        {
            RenderPassRequest req;
            req.target          = rt;
            req.clear           = clear;
            req.use_target_clear = false;
            req.delta_time      = deltaTime;
            req.cull_mode       = cull_mode;
            return RenderTo(req);
        }

        void ECSContext::OnResize(const VkExtent2D &extent)
        {
            HGL_CAPTURE_SCOPE();

            if (!active)
                return;

            LogInfo("[ECSContext] OnResize: %s %ux%u", GetName().c_str(), extent.width, extent.height);

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

            auto camera_system = GetSystem<CameraSystem>();
            if (camera_system)
            {
                camera_system->MarkAllCameraMatricesDirty();
                camera_system->ForceRefreshSelectedCamera();
            }
        }

        void ECSContext::Render(float deltaTime, const std::function<void(float)> &pre_render)
        {
            // 只在场景结构变化（scene_structure_dirty）时重新 gather，
            // 避免每帧全量遍历所有 entity/component（结构稳定时零开销）。
            if (scene_structure_dirty)
            {
                SceneStats stats = GatherSceneStats(this);
                uint64_t current_hash = stats.GetHash();

                if (current_hash != cached_adaptive_scene_hash)
                {
//                        LogDebug("[ECS] Adaptive RenderGraph scene hash changed: %llu -> %llu, regenerating",cached_adaptive_scene_hash, current_hash);
                    cached_adaptive_render_graph = CreateAdaptiveRenderGraph(this, stats);
                    cached_adaptive_scene_hash = current_hash;
                }

                scene_structure_dirty = false;
            }

            Render(deltaTime, cached_adaptive_render_graph, pre_render);
        }

        void ECSContext::RenderPreBeginFrame(float deltaTime)
        {
            if (!active)
                return;

            // Run all phases that execute before the command buffer opens:
            // PreBeginFrame（EnvironmentSystem/RenderTargetSystem 等）
            RunRenderPhaseUpdates(ExecutionPhase::RenderPreBeginFrame,  deltaTime);
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

            if (!RecreateSwapchainIfNeeded())
            {
                if (auto *target = GetRenderTarget())
                {
                    auto *swapchain_rt = dynamic_cast<graph::SwapchainRenderTarget *>(target);
                    if (swapchain_rt && swapchain_rt->NeedsResize())
                        return false;
                }
            }

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

            if (!swapchain_ok)
                RecreateSwapchainIfNeeded();

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
        }

        void ECSContext::PrepareRenderPassSetup(uint32_t frameIndex, float deltaTime)
        {
            if (!active)
                return;

            // Strict enum order — all CPU work and GPU uploads happen
            // before BeginRenderPass; the render pass only issues draw commands.
            SetFrameIndex(frameIndex);
            RunRenderPhaseUpdates(ExecutionPhase::RenderCollect,     deltaTime); // collect / cull visible components
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

            const bool recreated = RecreateSwapchainIfNeeded();
            return submit_ok || recreated;
        }

        bool ECSContext::RecreateSwapchainIfNeeded()
        {
            auto *target = GetRenderTarget();
            auto *swapchain_rt = dynamic_cast<graph::SwapchainRenderTarget *>(target);
            if (!swapchain_rt || !swapchain_rt->NeedsResize() || !graphics_context)
                return false;

            auto *module_manager = graphics_context->GetModuleManager();
            auto *swapchain_module = module_manager ? module_manager->Get<graph::SwapchainModule>() : nullptr;
            if (!swapchain_module)
                return false;

            VkExtent2D current_extent = swapchain_rt->GetExtent();
            swapchain_module->OnResize(current_extent);
            render_target = swapchain_module->GetRenderTarget();
            if (render_target)
            {
                OnResize(render_target->GetExtent());
            }
            return render_target != nullptr;
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

        void ECSContext::RunRenderPhaseUpdates(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime)
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

//            HGL_CAPTURE_SCOPE();
//            LogDebug("[ECS] Update Begin: %s", system->GetName().c_str());

            system->Update(deltaTime);

//            LogDebug("[ECS] Update End: %s", system->GetName().c_str());
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

//                HGL_CAPTURE_SCOPE();
//                LogDebug("[ECS] Render Begin: %s (phase %d)", entry.system->GetName().c_str(), entry.phase);

                entry.system->Render(current_render_cmd, deltaTime);

//                LogDebug("[ECS] Render End: %s", entry.system->GetName().c_str());
            }

            if (gpu_device)
                gpu_device->SetDrawPhaseActive(false);
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

                LogWarning("[ECSContext::SortSystemList] %s system dependencies contain a cycle. Falling back to phase/insertion order.",label);
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

            auto& sys_map = effective_is_render ? render_systems : tick_systems;
            auto& order_list = effective_is_render ? render_system_order : tick_system_order;
            auto& dirty_flag = effective_is_render ? render_order_dirty : tick_order_dirty;
            auto& deps = effective_is_render ? render_dependencies : tick_dependencies;

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
            frame_index = index;

            // 世界私有直推：只推进本世界 TransformSystem 的 L2W ring 帧索引
            // （旧静态广播会触达所有世界的 buffer，且静态表析构不摘除留悬空）
            auto ts = GetSystem<TransformSystem>();
            if (ts)
                if (auto *tb = ts->GetTransformBuffer())
                    tb->SetFrameIndex(index);
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

                auto& count = system_group_component_counts[group];
                ++count;

                EnsureSystemGroupSystems(this, group, GetRenderTarget());
                SetElementTypeSystemsEnabled(group, true);
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

                auto it = system_group_component_counts.find(group);
                if (it != system_group_component_counts.end())
                {
                    if (it->second > 0)
                        --(it->second);

                    if (it->second == 0)
                    {
                        SetElementTypeSystemsEnabled(group, false);
                    }

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
    }//namespace ecs
}//namespace hgl
