#include<hgl/ecs/core/Context.h>

#include<utility>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/ecs/core/EntityManager.h>
#include<hgl/ecs/core/DefaultSystems.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/ecs/components/ShadowProxy.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/support/PrimitiveState.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/support/RenderPipelineBase.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/ecs/support/RenderItemDataStorage.h>
#include<hgl/ecs/support/DrawItemIDStorage.h>
#include<hgl/ecs/support/CameraSlotGuard.h>
#include<hgl/ecs/support/CameraInfoStorage.h>
#include<hgl/graph/ubo/WorldAddresses.h>
#include<hgl/vk/buffer/StructView.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/graph/module/BufferManager.h>
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
#include<hgl/graph/module/EnvironmentManager.h>
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
            , bounding_box_storage(std::make_unique<BoundingBoxDataStorage>())
            , visibility_storage(std::make_unique<VisibilityDataStorage>())
            , render_item_storage(std::make_unique<RenderItemDataStorage>())
            , draw_item_id_storage(std::make_unique<DrawItemIDStorage>())
            , material_variant_table(std::make_unique<MaterialVariantTable>())
            , material_runtime_table(std::make_unique<MaterialRuntimeTable>())
            , camera_info_storage(std::make_shared<CameraInfoStorage>())
            , active(false)
        {
            // 可见性存储需要拿到本世界指针：祖先链上溯要回查实体与变换行
            visibility_storage->SetContext(this);

            // 材质运行期表需要拿到本世界指针：共享行回收时退休其纹理配置池行
            // （GPU 侧绑定随行存亡）；单测里没有世界，钩子自动 no-op。
            material_runtime_table->SetContext(this);
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
            if (camera_info_storage && !camera_info_storage->IsReady())
            {
                if (!camera_info_storage->Create(device, GetName()))
                {
                    LogError("[ECSContext::Initialize] 世界相机行存储（CameraInfoStorage）创建失败");
                    return false;
                }
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
                    transform_system = RegisterTickSystem<TransformSystem>();
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
            if (world_addresses_table)
            {
                delete world_addresses_table;
                world_addresses_table = nullptr;
            }

            if (world_addresses_buffer)
            {
                if (auto *gc = GetGraphicsContext())
                {
                    if (auto *bm = gc->GetBufferManager())
                        bm->Release(world_addresses_buffer);
                }
                world_addresses_buffer = nullptr;
            }

            world_addresses_addr = 0;

            // 常驻 fallback 相机随世界销毁（不是 Entity 组件 ⇒ 不走 entity_manager->Clear()）；
            // 它若正占着 0 号槽，槽账目已随相机表一起销毁 —— 不再有第二份 default_camera 要清。
            fallback_camera = nullptr;

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
            // （fixed_pixel 等像素缩放）由 gizmo 的
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
            // per-frame 数据槽 = **当前 RT** 的槽（主帧 = 交换链槽 [0,4)，离屏 pass = 该 RT 的槽带 [4,8)），
            // 两个方向天然不相交 ⇒ prepass 不再覆写主帧在途的 ring 槽（T10 根因）。
            PrepareRenderPassSetup(render_target ? render_target->GetCurrentFrameIndex() : 0u, deltaTime);

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

            // A1：本次离屏提交在 GPU 侧等待主帧车道（上一次主帧提交的值）——
            // 等效替代"上沿 CPU 等待"在 GPU 排序上的职责（CPU 侧写保护仍由 T3 解决）。
            if (saved_target)
            {
                if (graph::Semaphore *main_lane = saved_target->GetMainLane())
                {
                    const uint64_t main_value = saved_target->GetMainLaneValue();

                    if (main_value > 0)
                    {
                        const graph::SemaphoreSubmit main_wait = graph::SemaphoreSubmit::Wait(main_lane, main_value);
                        SetSubmitWaits(&main_wait, 1);
                    }
                }
            }

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

                // 覆盖相机的槽在 Update → BindCameraResources → EnsureCameraSlot 里认领；万一没认领到
                // （行池未就绪 / 槽耗尽 / 相机属于别的世界被拒）就退回 0 号槽，
                // 别把非法槽号一路带到行号算术里。槽号一律问世界（组件不再自持槽号）。
                const bool camera_in_this_world = (req.camera->world_owner == nullptr)
                                               || (req.camera->world_owner == this);
                const uint32_t override_camera_slot = camera_in_this_world
                                                        ? GetCameraSlot(req.camera)
                                                        : CameraInfoStorage::INVALID_SLOT;
                active_camera_id = CameraInfoStorage::IsValidSlot(override_camera_slot)
                                     ? override_camera_slot
                                     : CameraInfoStorage::kDefaultCameraSlot;
            }
            else
            {
                // 无覆盖相机：本 pass 用"本世界默认相机"（0 号槽；三级解析已保证有）
                active_camera_id = CameraInfoStorage::kDefaultCameraSlot;
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
                else
                {
                    const graph::CameraInfo *cam = GetActiveCameraInfo();
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
                // 本 pass 的 per-frame 数据槽 = 该 RT 的槽带（与主帧槽不相交）
                SetFrameIndex(render_target ? render_target->GetCurrentFrameIndex() : 0u);

                // 本次 pass 的相机行：**按 req.camera 直接发布**。阴影光源相机是**系统内建相机**
                // （EnvironmentSystem 直接 make_shared，不经 Entity/AddComponent 注册）⇒ 不在
                // component_registry 里，本世界 CollectCameras() 看不到它，靠通用发布会漏 ⇒
                // shadow pass 用退化相机渲染（阴影整体消失、receive_shadow 拨动无像素变化）。
                // （历史注释曾误记为"光源相机属另一个世界"——真因与定稿见
                //  doc/world-addresses-and-camera-model-plan.md §3/§7。）
                // 契约：离屏 pass 的相机行必须落在**离屏槽带**（[HGL_FRAME_SLOT_MAIN, TOTAL)）。
                // 若本 pass 的 RT 槽落在主帧带，离屏相机会覆写主帧在途的相机行
                // ⇒ 主画面渲成离屏/光源相机视角（**只出现一帧**、间歇：主帧读到自己那一槽被覆写的行）。
                if (req.camera && frame_index < HGL_FRAME_SLOT_MAIN)
                {
                    LogError("[ECSContext::RenderTo] 离屏 pass 的相机行落在主帧槽带：frame_index=%u "
                             "camera=\"%s\" slot=%u（主帧在途的相机行会被这个 pass 覆写）",
                             frame_index, req.camera->GetName().c_str(), GetCameraSlot(req.camera));
                }

                if (auto cs = GetSystem<CameraSystem>())
                    cs->PublishCamera(req.camera, frame_index);

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

            // A1：本帧离屏 RT 的车道值累积给主帧提交等（RenderTargetData::Submit 已 signal）
            if (ok)
                AddFrameLaneWait(rt->GetLane(), rt->GetLaneValue());

            SetSubmitWaitsFromFrameLanes();

            render_target = saved_target;

            // 归还主帧的 per-frame 数据槽（pass 期间被切到离屏槽）
            SetFrameIndex(saved_target ? saved_target->GetCurrentFrameIndex() : 0u);

            if (!req.use_target_clear)
                rt->SetClearColor(saved_clear);

            if (rts)
                rts->SetRenderTarget(rts_saved ? rts_saved : saved_target);

            if (use_camera_override)
            {
                camera_system->RestoreMainCamera();
            }

            // pass 退出：回到主帧的"本世界默认相机"（0 号槽；RestoreMainCamera 已把主相机重解算）
            active_camera_id = CameraInfoStorage::kDefaultCameraSlot;

            return ok;
        }

        void ECSContext::SetSubmitWaits(const graph::SemaphoreSubmit *waits,const uint32_t count)
        {
            submit_wait_count = 0;

            for(uint32_t i=0;i<count && submit_wait_count<MAX_LANE_WAITS;i++)
                if(waits[i].semaphore)
                    submit_waits[submit_wait_count++] = waits[i];
        }

        void ECSContext::AddFrameLaneWait(graph::Semaphore *lane,const uint64_t value)
        {
            if(!lane||value==0)
                return;

            if(frame_lane_wait_count>=MAX_LANE_WAITS)
            {
                LogWarning("[ECSContext] 车道等待列表已满(%u)，忽略 lane=%p value=%llu",
                           MAX_LANE_WAITS,static_cast<void *>(lane),
                           static_cast<unsigned long long>(value));
                return;
            }

            frame_lane_waits[frame_lane_wait_count++] = graph::SemaphoreSubmit::Wait(lane,value);
        }

        void ECSContext::SetSubmitWaitsFromFrameLanes()
        {
            SetSubmitWaits(frame_lane_waits,frame_lane_wait_count);
        }

        const graph::SemaphoreSubmit *ECSContext::TakeSubmitWaits(uint32_t &count,const bool is_main_frame)
        {
            count = submit_wait_count;
            submit_wait_count = 0;

            // 主帧提交即本帧收尾：清空累积，下一帧重新收集
            if(is_main_frame)
                frame_lane_wait_count = 0;

            return count>0?submit_waits:nullptr;
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

            // 相机行发布必须在这之后：本帧数据槽（frameIndex）此时才确定
            // （主帧 = acquire 之后拿到的交换链槽；离屏 pass = 该 RT 的槽带）。
            // 这里只覆盖本世界 CameraSystem 收集得到的相机（= 经 Entity 注册的那些）；系统内建
            // 相机（阴影光源相机等）不在其中，由 RenderTo 按 req.camera 单独发布（见下）。
            if (auto cs = GetSystem<CameraSystem>())
                cs->PublishCameraRows(frameIndex);

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

        bool ECSContext::InitializeWorldAddressesTable()
        {
            if (world_addresses_table)
                return true;

            auto *gc = GetGraphicsContext();
            if (!gc)
            {
                LogError("[ECSContext::InitializeWorldAddressesTable] graphics context is null");
                return false;
            }

            auto *bm = gc->GetBufferManager();
            if (!bm)
            {
                LogError("[ECSContext::InitializeWorldAddressesTable] buffer manager is null");
                return false;
            }

            // BDA 要求：必须以 SHADER_DEVICE_ADDRESS usage 创建（CreateUBO 的额外 usage 位是 0）。
            // 表本体一份、**按帧槽切成 kWorldAddressesSlotCount 份**：每帧只写自己那一槽。
            char name_buf[192];
            std::snprintf(name_buf, sizeof(name_buf),
                          "World:%s:AddressesTable", GetName().c_str());

            world_addresses_buffer = bm->CreateSSBO(
                name_buf,
                VkDeviceSize(graph::kWorldAddressesSlotStride) * graph::kWorldAddressesSlotCount);
            if (!world_addresses_buffer)
            {
                LogError("[ECSContext::InitializeWorldAddressesTable] CreateSSBO failed");
                return false;
            }

            world_addresses_buffer->SetUpdateClass(graph::BufferUpdateClass::Default);

            world_addresses_table =
                graph::StructView<graph::WorldAddresses>::Create(world_addresses_buffer, false);
            if (!world_addresses_table)
            {
                LogError("[ECSContext::InitializeWorldAddressesTable] StructView create failed");
                return false;
            }

            world_addresses_addr = gpu_device
                ? gpu_device->GetBufferDeviceAddressAligned16(world_addresses_buffer->GetBuffer())
                : 0;
            if (world_addresses_addr == 0)
            {
                // 地址缺失 = shader 解引用 0 基址 = UB / 设备丢失（0 VUID 抓不到）⇒ fail-fast
                LogError("[ECSContext::InitializeWorldAddressesTable] 世界地址表取不到设备地址（16B 对齐 / usage 检查）");
                return false;
            }

            // 整表清零（每槽）：未发布的字段保持 0，由消费者自行判定"无数据"。
            if (auto *base = reinterpret_cast<uint8_t *>(world_addresses_table->Data()))
            {
                for (uint32_t slot = 0; slot < graph::kWorldAddressesSlotCount; ++slot)
                    *reinterpret_cast<graph::WorldAddresses *>(
                        base + size_t(slot) * graph::kWorldAddressesSlotStride) = graph::WorldAddresses{};
            }

            world_addresses_table->Commit();

            SyncWorldAddresses();
            return true;
        }

        uint64_t ECSContext::GetWorldAddressesAddress(const uint32_t frame_slot) const
        {
            if (world_addresses_addr == 0)
                return 0;

            return world_addresses_addr
                 + uint64_t(frame_slot % graph::kWorldAddressesSlotCount)
                 * uint64_t(graph::kWorldAddressesSlotStride);
        }

        graph::EnvProfileID ECSContext::GetEnvProfileID() const
        {
            if (env_profile_explicit)
                return env_profile;

            // 未显式设置 ⇒ 跟随**本世界 RT**（作者侧 SetEnvironmentProfile 可能发生在世界创建之后，
            // 所以这里每帧解析而不是初始化时取一次）
            if (render_target)
                return render_target->GetEnvironmentProfile();

            return graph::kEnvProfileDefault;
        }

        void ECSContext::SetEnvProfileID(graph::EnvProfileID id, bool take_ownership)
        {
            // 换 profile 前先把**旧的、本世界拥有的**那个归还（否则换一次就漏一个 profile）
            if (env_profile_owned && env_profile != id)
                if (auto *manager = ResolveEnvManager())
                    manager->Release(env_profile);

            env_profile = id;
            env_profile_explicit = true;
            env_profile_owned = take_ownership;

            // 立刻重算本世界表的 sky / shadow 地址（不等到下一帧）：换 profile 的那一帧就生效
            SyncWorldAddresses();
        }

        graph::EnvProfileID ECSContext::CreateEnvProfile(const AnsiString &name,
                                                         const graph::EnvironmentInfo &init)
        {
            auto *manager = ResolveEnvManager();
            if (!manager)
                return graph::kEnvProfileInvalid;

            const graph::EnvProfileID id = manager->Create(name, init);
            if (id == graph::kEnvProfileInvalid)
                return id;

            SetEnvProfileID(id, true);      // 本世界创建的 ⇒ Shutdown 时归还
            return id;
        }

        graph::EnvironmentManager* ECSContext::ResolveEnvManager() const
        {
            graph::GraphicsContext *gc = nullptr;
            if (auto *rc = const_cast<ECSContext *>(this)->GetRenderContext())
                gc = rc->GetGraphicsContext();
            if (!gc)
                gc = const_cast<ECSContext *>(this)->GetGraphicsContext();

            return gc ? gc->GetEnvironmentManager() : nullptr;
        }

        bool ECSContext::SyncWorldAddresses()
        {
            if (!world_addresses_table)
                return false;

            auto *base = reinterpret_cast<uint8_t *>(world_addresses_table->Data());
            if (!base)
                return false;

            const uint32_t slot = frame_index % graph::kWorldAddressesSlotCount;

            // 槽步长是上界（可能大于 sizeof）⇒ 按字节偏移取槽。
            auto *dst = reinterpret_cast<graph::WorldAddresses *>(
                base + size_t(slot) * graph::kWorldAddressesSlotStride);

            graph::WorldAddresses want{};

            if (camera_info_storage && camera_info_storage->IsReady())
                want.addr_camera_info = camera_info_storage->GetGPUBase();

            if (auto *storage = GetRenderItemStorage())
                want.addr_global_render_items = storage->GetGPUAddress();

            if (auto *id_storage = GetDrawItemIDStorage())
                want.addr_draw_item_ids = id_storage->GetGPUAddress();

            // env（sky / shadow）随世界（C2）：按**本世界生效 profile** 取址写本世界槽。
            // sky 单份 buffer（全帧槽同址）；shadow 是每帧槽一份的 ring（下标 = 帧槽）⇒ 取本槽那份。
            // 取不到地址（0）时保留上一份好值——0 地址会让读 sky/shadow 的 shader 解引用 0 基址（UB）。
            const graph::EnvProfileID active_env_profile = GetEnvProfileID();
            if (auto *env_manager = ResolveEnvManager())
            {
                const uint64_t sky_addr = env_manager->GetSkyAddress(active_env_profile);
                if (sky_addr != 0)
                    want.addr_sky = sky_addr;

                const uint64_t shadow_addr = env_manager->GetShadowAddress(active_env_profile, slot);
                if (shadow_addr != 0)
                    want.addr_shadow = shadow_addr;
            }

            const uint64_t old_sky = dst->addr_sky;
            const uint64_t old_shadow = dst->addr_shadow;

            if (std::memcmp(dst, &want, sizeof(graph::WorldAddresses)) == 0)
                return false;

            *dst = want;
            world_addresses_table->Commit();

            // env 地址变化时留一行痕（诊断"两世界同帧各自的 sky/shadow 是否真的分开"）
            if (old_sky != want.addr_sky || old_shadow != want.addr_shadow)
                LogInfo("[ECSContext::SyncWorldAddresses] 世界 \"%s\" env 入表：profile=%u "
                        "sky=0x%llX shadow[%u]=0x%llX",
                        GetName().c_str(), active_env_profile,
                        static_cast<unsigned long long>(want.addr_sky), slot,
                        static_cast<unsigned long long>(want.addr_shadow));

            return true;
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

            // 帧槽变了 ⇒ 本世界的地址表换到本槽（相机行表 / 渲染项表 / DrawItemID 表随世界，
            // 每个 pass 各写自己的槽，不与在途帧/其它世界互踩）。
            SyncWorldAddresses();
        }

        uint32_t ECSContext::GetActiveCameraRow() const
        {
            // 槽号非法（本帧尚未解析出相机认领的槽 / 槽耗尽）⇒ 退回 **0 号槽**（默认相机 / 常驻 fallback）：
            // 着色器拿到的永远是一个合法行，而不是按 INVALID_SLOT 算出的越界行（会读到别的世界的存储）。
            const uint32_t slot = CameraInfoStorage::IsValidSlot(active_camera_id)
                                    ? active_camera_id
                                    : CameraInfoStorage::kDefaultCameraSlot;

            // 行号 = 相机槽 × 帧槽总数 + 帧槽（世界内算术，见 CameraInfoStorage::CameraRow）
            return CameraInfoStorage::CameraRow(slot, frame_index);
        }

        const graph::CameraInfo* ECSContext::GetActiveCameraInfo()
        {
            auto camera_system = GetSystem<CameraSystem>();
            return camera_system ? camera_system->GetActiveCameraInfo() : nullptr;
        }

        // ==== 相机（世界级资源：16 槽 × 帧槽，0 号槽恒为本世界默认相机）====
        // 「相机属于哪个世界 / 哪个槽」的唯一真源 = camera_info_storage 的槽账目（槽 → 宿主弱引用）。

        CameraComponent* ECSContext::GetCamera(const uint32_t slot) const
        {
            return camera_info_storage ? camera_info_storage->GetSlotOwner(slot) : nullptr;
        }

        CameraComponent* ECSContext::GetDefaultCamera() const
        {
            // 默认相机 = **0 号槽的宿主**（没有第二份 weak_ptr 记账：相机销毁 ⇒ 弱引用失效 ⇒
            // 这里自然返回 nullptr，下一次三级解析重新指名）。
            return GetCamera(CameraInfoStorage::kDefaultCameraSlot);
        }

        bool ECSContext::SetDefaultCamera(const std::shared_ptr<CameraComponent>& camera)
        {
            if (!camera_info_storage)
                return false;

            // 0 号槽唯一：改绑即顶替原宿主（常驻 fallback 被实体相机顶替时**不需要**额外动作）。
            return camera_info_storage->BindCameraSlot(CameraInfoStorage::kDefaultCameraSlot, camera);
        }

        uint32_t ECSContext::GetCameraSlot(const CameraComponent* camera) const
        {
            if (!camera || !camera_info_storage)
                return CameraComponent::kInvalidSlot;

            return camera_info_storage->FindSlot(camera);
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
                return entity ? entity->GetComponent<T>().get() : nullptr;
            }

            template<typename T>
            inline const T *ComponentOfEntity(const Entity *entity)
            {
                return entity ? entity->GetComponent<T>().get() : nullptr;
            }

            template<typename T, typename... Args>
            inline T *GetOrAddComponentOfEntity(Entity *entity, Args&&... args)
            {
                if (!entity)
                    return nullptr;

                auto component = entity->GetComponent<T>();

                if (!component)
                    component = entity->AddComponent<T>(std::forward<Args>(args)...);

                return component.get();
            }
        }

        // 实体 → 相机（世界级读取口；与 GetMaterialData / GetGeometryData 同族）。
        // 相机是**世界级资源**（槽账目在 camera_info_storage），实体只是宿主：
        // 示例/系统取相机一律走这里，不走 Entity::GetComponent<CameraComponent>
        // （那会形成第二个读取入口 ⇒ 组件级旧读法 vs 世界级访问器成双真值）。
        const CameraComponent* ECSContext::GetCameraByEntity(const EntityID owner) const
        {
            return ComponentOfEntity<CameraComponent>(GetEntity(owner));
        }

        CameraComponent* ECSContext::GetCameraByEntity(const EntityID owner)
        {
            return ComponentOfEntity<CameraComponent>(GetEntity(owner));
        }

        const CameraComponent* ECSContext::GetCameraByEntity(const Entity *owner) const
        {
            return ComponentOfEntity<CameraComponent>(owner);
        }

        CameraComponent* ECSContext::GetCameraByEntity(Entity *owner)
        {
            return ComponentOfEntity<CameraComponent>(owner);
        }

        CameraComponent* ECSContext::GetOrCreateCamera(const EntityID owner, const std::string& name)
        {
            return GetOrAddComponentOfEntity<CameraComponent>(GetEntity(owner), name);
        }

        CameraComponent* ECSContext::GetOrCreateCamera(Entity *owner, const std::string& name)
        {
            return GetOrAddComponentOfEntity<CameraComponent>(owner, name);
        }

        bool ECSContext::ReleaseCameraSlot(const CameraComponent* camera)
        {
            if (!camera_info_storage)
                return false;

            return camera_info_storage->ReleaseCameraSlot(camera);
        }

        bool ECSContext::EnsureCameraSlot(CameraComponent* camera, const bool is_default)
        {
            if (!camera || !camera_info_storage)
                return false;

            // 槽的宿主必须是 shared_ptr 持有者（槽账目 = 宿主弱引用）：实体组件（注册表）/ 作者 /
            // 系统（CameraSlotGuard）都满足；栈上或裸 new 的相机拿不到槽 ⇒ fail-fast 留痕
            // （宁可它不出图，也不能按越界行号写到别的世界去）。
            auto owner = std::static_pointer_cast<CameraComponent>(camera->weak_from_this().lock());
            if (!owner)
            {
                GLogError("[ECSContext] 相机 \"%s\" 没有 shared_ptr 宿主，无法认领相机槽"
                          "（槽账目按宿主弱引用计占用期）", camera->GetName().c_str());
                return false;
            }

            if (is_default)
            {
                // 0 号槽 = 本世界默认相机专属（三级解析的落点，含常驻 fallback）。
                // 「升格为默认相机」时若它原持**普通槽**，先把旧槽交回（否则那个槽会一直挂在这个
                // 相机名下、迟早把 16 个槽顶满）；0 号槽唯一 ⇒ 改绑即让原宿主（含常驻 fallback）让位。
                const uint32_t old_slot = camera_info_storage->FindSlot(camera);
                if (old_slot != CameraInfoStorage::INVALID_SLOT
                 && old_slot != CameraInfoStorage::kDefaultCameraSlot)
                    camera_info_storage->ReleaseCameraSlot(old_slot);

                if (!SetDefaultCamera(owner))
                    return false;

                camera->world_owner = this;
                return true;
            }

            if (camera_info_storage->FindSlot(camera) != CameraInfoStorage::INVALID_SLOT)
                return true;    // 已认领（幂等）

            if (!camera_info_storage->IsReady())
                return false;   // 行池未就绪：保持未认领 ⇒ 发布时跳过并一次性告警（fail-fast）

            const uint32_t slot = camera_info_storage->AcquireCameraSlot(owner);

            if (slot == CameraInfoStorage::INVALID_SLOT)
            {
                GLogError("[ECSContext] 世界相机槽耗尽（上限 %u）：相机 \"%s\" 未拿到槽，"
                          "本帧不会发布它的相机行——检查是否有相机未归还",
                          CameraInfoStorage::kSlotCapacity, camera->GetName().c_str());
                return false;
            }

            camera->world_owner = this;
            return true;
        }

        CameraComponent* ECSContext::EnsureFallbackCamera()
        {
            // 已存在 ⇒ 重新回到"默认相机"位置（可能被实体相机顶替过一轮，现在世界又没相机了）：
            // 重新成为 0 号槽的宿主。
            if (fallback_camera)
            {
                SetDefaultCamera(fallback_camera);
                return fallback_camera.get();
            }

            // 三级解析第 ④ 级：世界内没有任何相机组件，但渲染**必须**有一个相机 ⇒ 在 (0,0,0) 强制生成。
            // 常驻（创建后一直活着）、占 0 号槽、**不进 component_registry**（CollectCameras() 看不到它，
            // 因此不会参与"最小 EntityID.index"的选主，也不会被当作实体相机的重复项）。
            auto camera = std::make_shared<CameraComponent>("WorldFallbackCamera");
            camera->position       = math::Vector3f(0.0f, 0.0f, 0.0f);
            camera->target         = math::Vector3f(0.0f, 0.0f, 1.0f);
            camera->is_main_camera = false;
            camera->matrix_dirty   = true;
            camera->world_owner    = this;

            fallback_camera = camera;
            SetDefaultCamera(camera);

            GLogInfo("[ECS] %s: 世界内没有相机 ⇒ 在 (0,0,0) 生成常驻 fallback 相机（占 0 号槽）",
                     GetName().c_str());

            return fallback_camera.get();
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

        void ECSContext::RegisterTransform(TransformID id,bool isMovable)
        {
            if (!IsValidTransformID(id))
                return;

            // 有变换出现即自动装上 TransformSystem（避免样例手动注册）
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

            auto &list = isMovable ? movable_transforms : static_transforms;

            if (std::find(list.begin(),list.end(),id) == list.end())
                list.push_back(id);
        }

        void ECSContext::MigrateTransform(TransformID id,bool toMovable)
        {
            if (!IsValidTransformID(id))
                return;

            auto remove_from_list = [id](std::vector<TransformID>& list)
            {
                list.erase(std::remove(list.begin(),list.end(),id),list.end());
            };

            remove_from_list(static_transforms);
            remove_from_list(movable_transforms);

            auto &target = toMovable ? movable_transforms : static_transforms;

            if (std::find(target.begin(),target.end(),id) == target.end())
                target.push_back(id);

            if (auto *storage = GetTransformStorage())
                storage->SetMobility(id,toMovable ? 1 : 0);
        }

        void ECSContext::UnregisterTransform(TransformID id)
        {
            if (!IsValidTransformID(id))
                return;

            auto remove_from_list = [id](std::vector<TransformID>& list)
            {
                list.erase(std::remove(list.begin(),list.end(),id),list.end());
            };

            remove_from_list(static_transforms);
            remove_from_list(movable_transforms);
        }

        TransformID ECSContext::CreateTransform(EntityID owner,Mobility mobility)
        {
            auto *storage = GetTransformStorage();

            if (!storage)
                return INVALID_TRANSFORM_ID;

            const TransformID id = storage->Allocate();

            storage->SetOwner(id,owner);
            storage->SetMobility(id,(mobility == Mobility::Movable) ? 1 : 0);

            RegisterTransform(id,mobility == Mobility::Movable);

            return id;
        }

        void ECSContext::DestroyTransform(TransformID id)
        {
            if (!IsValidTransformID(id))
                return;

            UnregisterTransform(id);

            if (auto *storage = GetTransformStorage())
            {
                // 先从父的子表里摘掉自己（父链与子表真源都在存储），再释放本行
                const TransformID parent = storage->GetParent(id);

                if (IsValidTransformID(parent))
                    storage->RemoveChild(parent,id);

                storage->Deallocate(id);
            }
        }

        TransformID ECSContext::GetTransformID(EntityID owner) const
        {
            const auto *storage = GetTransformStorage();

            return storage ? storage->FindByOwner(owner) : INVALID_TRANSFORM_ID;
        }

        TransformAccessor ECSContext::GetTransform(TransformID id)
        {
            return TransformAccessor(GetTransformStorage(),id,this);
        }

        BoundingBoxAccessor ECSContext::GetBoundingBoxByEntity(EntityID owner) const
        {
            auto *storage = bounding_box_storage.get();

            if (!storage)
                return BoundingBoxAccessor();

            // const 成员里仍要交出可变 storage 句柄（行数据由世界拥有；句柄只做定位）
            return BoundingBoxAccessor(storage,storage->FindByOwner(owner),const_cast<ECSContext *>(this));
        }

        BoundingBoxAccessor ECSContext::GetOrCreateBoundingBox(EntityID owner)
        {
            auto *storage = bounding_box_storage.get();

            if (!storage)
                return BoundingBoxAccessor();

            auto id = storage->FindByOwner(owner);

            if (id == BoundingBoxDataStorage::INVALID_HANDLE)
            {
                id = storage->Allocate();
                storage->SetOwner(id,owner);
            }

            return BoundingBoxAccessor(storage,id,this);
        }

        MaterialData* ECSContext::GetMaterialData(EntityID owner)
        {
            return ComponentOfEntity<MaterialData>(GetEntity(owner));
        }

        const MaterialData* ECSContext::GetMaterialData(EntityID owner) const
        {
            return ComponentOfEntity<MaterialData>(GetEntity(owner));
        }

        MaterialData* ECSContext::GetMaterialData(Entity *owner)
        {
            return ComponentOfEntity<MaterialData>(owner);
        }

        const MaterialData* ECSContext::GetMaterialData(const Entity *owner) const
        {
            return ComponentOfEntity<MaterialData>(owner);
        }

        MaterialData* ECSContext::GetOrCreateMaterialData(EntityID owner)
        {
            return GetOrAddComponentOfEntity<MaterialData>(GetEntity(owner));
        }

        MaterialData* ECSContext::GetOrCreateMaterialData(Entity *owner)
        {
            return GetOrAddComponentOfEntity<MaterialData>(owner);
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

        ShadowProxy* ECSContext::GetShadowProxy(EntityID owner)
        {
            return ComponentOfEntity<ShadowProxy>(GetEntity(owner));
        }

        const ShadowProxy* ECSContext::GetShadowProxy(EntityID owner) const
        {
            return ComponentOfEntity<ShadowProxy>(GetEntity(owner));
        }

        ShadowProxy* ECSContext::GetShadowProxy(Entity *owner)
        {
            return ComponentOfEntity<ShadowProxy>(owner);
        }

        const ShadowProxy* ECSContext::GetShadowProxy(const Entity *owner) const
        {
            return ComponentOfEntity<ShadowProxy>(owner);
        }

        void ECSContext::InvalidateEntityRuntimePipeline(EntityID owner)
        {
            if (!material_runtime_table)
                return;

            if (MaterialRuntimeSlot *slot = material_runtime_table->GetSlot(owner))
            {
                slot->runtime_pipeline_pass = nullptr;
                slot->runtime_pipeline      = nullptr;
            }
        }

        void ECSContext::DestroyBoundingBox(BoundingBoxDataStorage::HandleID id)
        {
            if (id == BoundingBoxDataStorage::INVALID_HANDLE)
                return;

            if (auto *storage = bounding_box_storage.get())
                storage->Deallocate(id);
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
