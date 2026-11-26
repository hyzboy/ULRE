#include <hgl/ecs/support/render/WorldRenderRunner.h>
#include <hgl/ecs/core/render/RenderItem.h>
#include <hgl/ecs/core/render/MaterialBatch.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/systems/render/frame/RenderSystemCore.h>
#include <hgl/ecs/systems/render/frame/RenderTargetSystem.h>
#include <hgl/ecs/systems/render/buffer/RenderSceneUBOSystem.h>
#include <hgl/ecs/systems/render/light_shadow/MainLightShadowSystem.h>
#include <hgl/ecs/systems/render/frame/SwapchainNextImageSystem.h>
#include <hgl/ecs/systems/render/frame/SwapchainSubmitSystem.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/ecs/support/storage/CameraInfoStorage.h>
#include <hgl/ecs/support/storage/FrameProfiler.h>
#include <hgl/ecs/support/pipeline/PrimitiveState.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/vk/VKRenderTargetSwapchain.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/GraphModuleManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/SwapchainModule.h>
#include <hgl/graph/CameraInfo.h>
#include <hgl/time/Time.h>
#include <hgl/common/DebugChannel.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    namespace
    {
        struct PrepassTimingAccum
        {
            bool     enabled            = false;
            uint32_t print_interval     = 60;
            uint64_t last_main_frame_ns = 0;
            uint64_t calls = 0, t_total_ns = 0, t_up_ns = 0, t_down_ns = 0;
            uint64_t t_begin_ns = 0, t_draw_ns = 0, t_submit_ns = 0;
            uint64_t frames_total = 0;
            uint32_t frames       = 0;
            uint64_t t_frame_ns   = 0;
        };

        PrepassTimingAccum g_prepass_timing;

        inline uint64_t PrepassTimingNow()
        {
            return static_cast<uint64_t>(hgl::GetUptimeUs() * 1000ULL);
        }

        void PrepassTimingInit()
        {
            static bool s_inited = false;
            if (s_inited)
                return;

            s_inited = true;

            if (!ULRE_DEBUG_ON(Perf))
                return;

            g_prepass_timing.enabled = true;

            const int64_t interval =
                hgl::debug::ConfigInt("perf", "prepass_interval", 60);

            if (interval > 0 && interval <= 0xFFFF)
                g_prepass_timing.print_interval = static_cast<uint32_t>(interval);

            GLogInfo("[PREPASS-TIMING] 启用（ULRE_DEBUG.toml 的 channels.perf=true，"
                     "间隔 [perf] prepass_interval=%u）：每 %u 个主帧报一次均值；"
                     "本引擎无 GPU 时间戳 ⇒ 量的是 CPU 可见账（两处 fence 等待 + 其余）",
                     g_prepass_timing.print_interval, g_prepass_timing.print_interval);
        }

        void PrepassTimingFrameBoundary()
        {
            PrepassTimingInit();

            PrepassTimingAccum &T = g_prepass_timing;
            if (!T.enabled)
                return;

            const uint64_t now = PrepassTimingNow();
            if (T.last_main_frame_ns != 0)
            {
                T.t_frame_ns += now - T.last_main_frame_ns;
                ++T.frames;
                ++T.frames_total;
            }
            T.last_main_frame_ns = now;

            if (T.frames < T.print_interval)
                return;

            const double f          = static_cast<double>(T.frames);
            const double frame_us   = static_cast<double>(T.t_frame_ns) / 1000.0 / f;
            const double calls_pf   = static_cast<double>(T.calls) / f;
            const double total_us   = static_cast<double>(T.t_total_ns) / 1000.0 / f;
            const double up_us      = static_cast<double>(T.t_up_ns) / 1000.0 / f;
            const double down_us    = static_cast<double>(T.t_down_ns) / 1000.0 / f;
            const double begin_us  = static_cast<double>(T.t_begin_ns) / 1000.0 / f;
            const double draw_us   = static_cast<double>(T.t_draw_ns) / 1000.0 / f;
            const double submit_us = static_cast<double>(T.t_submit_ns) / 1000.0 / f;
            const double rest_us   = total_us - up_us - down_us - begin_us - draw_us - submit_us;
            const double blocked_us = up_us + down_us;

            GLogInfo("[PREPASS-TIMING] 主帧#%llu（窗口 %u 帧）：主帧均值=%.0fus | 离屏 pass=%.2f 次/帧、"
                     "合计=%.0fus/帧〔begin=%.0fus（collect/装配/派发） draw=%.0fus submit=%.0fus "
                     "| 上沿等主帧=%.0fus 下沿等自身=%.0fus | 其余=%.0fus〕"
                     "阻塞=%.0fus（占总 %.1f%%）⇒ 占主帧 %.1f%%",
                     static_cast<unsigned long long>(T.frames_total), T.frames, frame_us, calls_pf,
                     total_us, begin_us, draw_us, submit_us, up_us, down_us, rest_us,
                     blocked_us,
                     total_us > 0.0 ? blocked_us / total_us * 100.0 : 0.0,
                     frame_us > 0.0 ? total_us / frame_us * 100.0 : 0.0);

            T.calls       = 0;
            T.t_total_ns  = 0;
            T.t_up_ns     = 0;
            T.t_down_ns   = 0;
            T.t_begin_ns  = 0;
            T.t_draw_ns   = 0;
            T.t_submit_ns = 0;
            T.frames     = 0;
            T.t_frame_ns = 0;
        }
    } // anonymous namespace

    WorldRenderRunner::WorldRenderRunner(ECSContext *ctx)
        : context(ctx)
    {
    }

    WorldRenderRunner::~WorldRenderRunner()
    {
        Shutdown();
    }

    void WorldRenderRunner::SetScenePipelineMode(ScenePipelineMode mode)
    {
        if (scene_pipeline_mode == mode)
            return;

        scene_pipeline_mode = mode;
        main_frame_topology.InvalidateSnapshot();
    }

    void WorldRenderRunner::ClearFrameCache()
    {
        render_frame_cache.renderItems.clear();
        render_frame_cache.cameraInfo = nullptr;
        render_frame_cache.renderableCount = 0;
    }

    void WorldRenderRunner::ClearMaterialBatches()
    {
        render_frame_cache.materialBatches.Clear();
    }

    void WorldRenderRunner::Shutdown()
    {
        ClearFrameCache();
        ClearMaterialBatches();
        render_core = nullptr;
        current_render_cmd = nullptr;
        frame_index = 0;
        render_submission_serial = 0;
    }

    void WorldRenderRunner::OnSwapchainTargetChanged(graph::IRenderTarget *target)
    {
        if (context)
            context->SetRenderTarget(target);
    }

    void WorldRenderRunner::OnRenderTargetResized(graph::IRenderTarget *target, uint32 width, uint32 height)
    {
        if (context && target && context->GetRenderTarget() == target)
            if (auto system = context->GetSystem<RenderSceneUBOSystem>())
                system->SetViewportExtent(width, height);
    }

    bool WorldRenderRunner::EnsureRenderCoreInitialized()
    {
        if (render_core)
            return true;

        render_core = new RenderSystemCore(context);
        if (!render_core->Initialize())
        {
            render_core = nullptr;
            return false;
        }

        return true;
    }

    bool WorldRenderRunner::HasPendingRenderableResources() const
    {
        if (!context)
            return false;

        ValueArray<GeometryData *> primitives;
        context->GetComponents<GeometryData>(primitives);

        for (GeometryData *geometry : primitives)
        {
            if (!geometry)
                continue;

            if (HasPendingMaterialResources(geometry->GetOwner()))
                return true;
        }

        ValueArray<GPUModelRefComponent *> model_refs;
        context->GetComponents<GPUModelRefComponent>(model_refs);
        for (GPUModelRefComponent *ref : model_refs)
        {
            if (!ref || ref->template_id == graph::ssbo::kInvalidModelTemplateID)
                continue;

            if (HasPendingMaterialResources(ref->GetOwner()))
                return true;
        }

        return false;
    }

    void WorldRenderRunner::ExecuteScenePrePassWorkflow(float deltaTime)
    {
        if (!context)
            return;

        switch (scene_pipeline_mode)
        {
            case ScenePipelineMode::StandardLitCSM:
            {
                if (auto shadow = context->GetSystem<MainLightShadowSystem>())
                {
                    if (shadow->IsMainLightShadowEnabled())
                    {
                        if (auto cam_sys = context->GetSystem<CameraSystem>())
                        {
                            if (auto *main_cam = cam_sys->GetMainCameraComponent())
                            {
                                shadow->RenderMainLightShadowPass(main_cam, deltaTime);
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
                break;

            case ScenePipelineMode::Custom:
            default:
                break;
        }
    }

    bool WorldRenderRunner::BeginManagedRenderFrame(float deltaTime, const bool need_swapchain_acquire, const graph::RenderPassOptions *options)
    {
        if (!context || !context->IsActive())
            return false;

        if (need_swapchain_acquire)
            PrepassTimingFrameBoundary();

        if (need_swapchain_acquire)
        {
            FrameProfileSpan prepass_span(FrameProfileSlot::ScenePrePass);
            ExecuteScenePrePassWorkflow(deltaTime);
        }

        if (!EnsureRenderCoreInitialized())
            return false;

        if (context->GetRenderTarget() && need_swapchain_acquire)
        {
            FrameProfileSpan acquire_span(FrameProfileSlot::AcquireSwapchain);
            if (!AcquireSwapchainImage(deltaTime))
            {
                GLogWarning("[WorldRenderRunner] AcquireSwapchainImage FAILED");
                return false;
            }
        }

        {
            FrameProfileSpan prebegin_span(FrameProfileSlot::PreBeginFrame);
            RenderPreBeginFrame(deltaTime);
            SyncRenderTargetViewportToExtent();

            if (auto *gc = context->GetGraphicsContext())
            {
                if (auto *tm = gc->GetTextureManager())
                {
                    tm->UpdateUploadQueue(gc->GetBindlessTextureManager());
                }
            }
        }

        {
            FrameProfileSpan begin_span(FrameProfileSlot::RenderCoreBegin);
            if (!render_core->BeginFrame())
            {
                GLogWarning("[WorldRenderRunner] BeginFrame FAILED");
                return false;
            }
        }

        SetCurrentRenderCmd(render_core->GetRenderCmd());
        auto *target = context->GetRenderTarget();
        PrepareRenderPassSetup(target ? target->GetCurrentFrameIndex() : 0u, deltaTime);

        {
            FrameProfileSpan pass_begin_span(FrameProfileSlot::BeginRenderPass);
            if (!render_core->BeginRenderPass(options))
            {
                GLogWarning("[WorldRenderRunner] BeginRenderPass FAILED");
                render_core->EndFrame();
                SetCurrentRenderCmd(nullptr);
                return false;
            }
        }

        return true;
    }

    bool WorldRenderRunner::EndManagedRenderFrame(float deltaTime)
    {
        if (!render_core)
            return false;

        bool ended = false;
        {
            FrameProfileSpan end_span(FrameProfileSlot::EndFrame);
            ended = render_core->EndFrame();
        }

        SetCurrentRenderCmd(nullptr);
        if (!ended)
            return false;

        bool submitted = false;
        {
            FrameProfileSpan submit_span(FrameProfileSlot::SubmitPresent);
            submitted = SubmitFrameToRenderTarget(deltaTime);
            if (!submitted)
            {
                GLogError("[WorldRenderRunner] SubmitFrameToRenderTarget FAILED");
            }
        }
        return submitted;
    }

    void WorldRenderRunner::RecordPreparedRenderPhaseRange(ExecutionPhase minPhase,
                                                          ExecutionPhase maxPhase,
                                                          float deltaTime,
                                                          bool submit_transforms,
                                                          const char *log_prefix)
    {
        if (!context)
            return;

        if (submit_transforms)
        {
            if (auto transform_system = context->GetSystem<TransformSystem>())
                transform_system->SubmitTransformUpdates();
        }

        if (log_prefix && ULRE_DEBUG_ON(RenderGraph))
        {
            GLogInfo("%s phase range %d to %d",
                     log_prefix,
                     static_cast<int>(minPhase),
                     static_cast<int>(maxPhase));
        }

        RunRenderSystemsInRange(minPhase, maxPhase, deltaTime);
    }

    void WorldRenderRunner::RenderDrawOnly(graph::RenderCmdBuffer* cmd, float deltaTime)
    {
        if (!context || !context->IsActive())
            return;

        if (!current_render_cmd && cmd)
            current_render_cmd = cmd;

        RecordPreparedRenderPhaseRange(ExecutionPhase::RenderCollect,
                                       ExecutionPhase::RenderStat,
                                       deltaTime,
                                       false,
                                       "[WorldRenderRunner::RenderDrawOnly]");

        if (current_render_cmd == cmd)
            current_render_cmd = nullptr;
    }

    bool WorldRenderRunner::RenderTo(const RenderPassRequest &req)
    {
        if (!context || !context->IsActive())
            return false;

        PrepassTimingInit();
        const uint64_t probe_t0 = g_prepass_timing.enabled ? PrepassTimingNow() : 0;
        uint64_t probe_up_ns = 0;
        uint64_t probe_down_ns = 0;
        uint64_t probe_begin_ns = 0;
        uint64_t probe_draw_ns = 0;
        uint64_t probe_submit_ns = 0;

        graph::IRenderTarget *rt = req.target;
        if (!rt)
        {
            GLogError("[WorldRenderRunner::RenderTo] render target is null");
            return false;
        }

        graph::IRenderTarget *saved_target = context->GetRenderTarget();

        if (saved_target)
        {
            if (graph::Semaphore *main_lane = saved_target->GetMainLane())
            {
                const uint64_t main_value = saved_target->GetMainLaneValue();

                if (main_value > 0)
                {
                    const graph::SemaphoreSubmit main_wait = graph::SemaphoreSubmit::Wait(main_lane, main_value);
                    context->SetSubmitWaits(&main_wait, 1);

                    static bool s_a1_wait_logged = false;
                    if (!s_a1_wait_logged)
                    {
                        s_a1_wait_logged = true;
                        GLogInfo("[A1] 离屏提交挂上主帧车道等待 v=%llu（prepass 覆写阴影前，在途主帧已读完）",
                                 static_cast<unsigned long long>(main_value));
                    }
                }
            }
        }

        const uint64_t probe_up_begin = (probe_t0 != 0) ? PrepassTimingNow() : 0;
        if (saved_target)
            saved_target->WaitFence();
        if (probe_up_begin != 0)
            probe_up_ns = PrepassTimingNow() - probe_up_begin;

        const hgl::Color4f saved_clear = rt->GetClearColor();
        if (!req.use_target_clear)
            rt->SetClearColor(req.clear);

        auto rts = context->GetSystem<RenderTargetSystem>();
        graph::IRenderTarget *rts_saved = rts ? rts->GetRenderTarget() : nullptr;
        if (rts)
            rts->SetRenderTarget(rt);

        context->SetRenderTarget(rt);

        auto camera_system = context->GetSystem<CameraSystem>();
        const bool use_camera_override = (req.camera != nullptr && camera_system);

        struct RenderToStateGuard
        {
            ECSContext* ctx;
            WorldRenderRunner* runner;
            graph::IRenderTarget* saved_target;
            graph::IRenderTarget* current_rt;
            const hgl::Color4f saved_clear;
            bool restore_clear;
            RenderTargetSystem* rts;
            graph::IRenderTarget* rts_saved;
            CameraSystem* cs;
            bool use_override;

            ~RenderToStateGuard()
            {
                if (ctx)
                {
                    ctx->SetRenderTarget(saved_target);
                    if (runner)
                        runner->SetFrameIndex(saved_target ? saved_target->GetCurrentFrameIndex() : 0u);

                    if (restore_clear && current_rt)
                        current_rt->SetClearColor(saved_clear);

                    if (rts)
                        rts->SetRenderTarget(rts_saved ? rts_saved : saved_target);

                    if (use_override && cs)
                        cs->RestoreMainCamera();

                    if (runner)
                    {
                        runner->GetPassScopeState().SetCameraSlot(CameraInfoStorage::kDefaultCameraSlot);
                        runner->GetPassScopeState().SetMobilityFilter(-1);
                        runner->GetPassScopeState().ClearShadow();
                    }
                }
            }
        } state_guard{
            context,
            this,
            saved_target,
            rt,
            saved_clear,
            !req.use_target_clear,
            rts.get(),
            rts_saved,
            camera_system.get(),
            use_camera_override
        };

        if (use_camera_override)
        {
            camera_system->SetOverrideCamera(req.camera);
            camera_system->Update(req.delta_time);

            const bool camera_in_this_world = (req.camera->world_owner == nullptr)
                                           || (req.camera->world_owner == context);
            const uint32_t override_camera_slot = camera_in_this_world
                                                    ? context->GetCameraSlot(req.camera)
                                                    : CameraInfoStorage::INVALID_SLOT;
            pass_scope.SetCameraSlot(CameraInfoStorage::IsValidSlot(override_camera_slot)
                                 ? override_camera_slot
                                 : CameraInfoStorage::kDefaultCameraSlot);
        }
        else
        {
            pass_scope.SetCameraSlot(CameraInfoStorage::kDefaultCameraSlot);
        }

        pass_scope.SetMobilityFilter(req.mobility_filter);

        const bool foreign_target =
            req.target && (req.target != saved_target);

        pass_scope.BeginShadow(req.is_shadow_pass || foreign_target);

        if (req.is_shadow_pass)
        {
            if (req.shadow_reference_camera)
            {
                pass_scope.SetShadowOrigin(glm::vec3(req.shadow_reference_camera->position));
            }
            else
            {
                const graph::CameraInfo *cam = context->GetActiveCameraInfo();
                if (cam)
                    pass_scope.SetShadowOrigin(glm::vec3(cam->pos.x, cam->pos.y, cam->pos.z));
            }
        }

        graph::RenderPassOptions pass_options;
        pass_options.load_depth = req.load_depth;
        pass_options.use_scissor = req.use_scissor;
        pass_options.scissor = req.scissor;
        pass_options.clear_scissor_depth = req.clear_scissor_depth;
        pass_options.clear_depth_value = 0.0f;
        pass_options.cull_mode = static_cast<int>(req.cull_mode);

        const graph::RenderPassOptions *p_options =
            (req.load_depth || req.use_scissor || req.clear_scissor_depth
             || req.cull_mode != CullMode::Inherit) ? &pass_options : nullptr;

        bool ok = false;

        const uint64_t probe_begin_start = (probe_t0 != 0) ? PrepassTimingNow() : 0;

        if (BeginManagedRenderFrame(req.delta_time, false, p_options))
        {
            if (probe_begin_start != 0)
                probe_begin_ns = PrepassTimingNow() - probe_begin_start;

            auto *current_target = context->GetRenderTarget();
            SetFrameIndex(current_target ? current_target->GetCurrentFrameIndex() : 0u);

            if (req.camera && frame_index < HGL_FRAME_SLOT_MAIN)
            {
                GLogError("[WorldRenderRunner::RenderTo] 离屏 pass 的相机行落在主帧槽带：frame_index=%u "
                         "camera=\"%s\" slot=%u（主帧在途的相机行会被这个 pass 覆写）",
                         frame_index, req.camera->GetName().c_str(), context->GetCameraSlot(req.camera));
            }

            if (auto cs = context->GetSystem<CameraSystem>())
                cs->PublishCamera(req.camera, frame_index);

            const uint64_t probe_draw_start = (probe_t0 != 0) ? PrepassTimingNow() : 0;
            RenderDrawOnly(render_core->GetRenderCmd(), req.delta_time);
            if (probe_draw_start != 0)
                probe_draw_ns = PrepassTimingNow() - probe_draw_start;

            const uint64_t probe_submit_start = (probe_t0 != 0) ? PrepassTimingNow() : 0;
            ok = EndManagedRenderFrame(req.delta_time);
            if (probe_submit_start != 0)
                probe_submit_ns = PrepassTimingNow() - probe_submit_start;
        }

        pass_scope.SetMobilityFilter(-1);
        pass_scope.ClearShadow();

        const uint64_t probe_down_begin = (probe_t0 != 0) ? PrepassTimingNow() : 0;
        if (ok)
            rt->WaitFence();
        if (probe_down_begin != 0)
            probe_down_ns = PrepassTimingNow() - probe_down_begin;

        if (ok)
            context->AddFrameLaneWait(rt->GetLane(), rt->GetLaneValue());

        context->SetSubmitWaitsFromFrameLanes();

        if (probe_t0 != 0)
        {
            g_prepass_timing.calls       += 1;
            g_prepass_timing.t_total_ns  += PrepassTimingNow() - probe_t0;
            g_prepass_timing.t_up_ns     += probe_up_ns;
            g_prepass_timing.t_down_ns   += probe_down_ns;
            g_prepass_timing.t_begin_ns  += probe_begin_ns;
            g_prepass_timing.t_draw_ns   += probe_draw_ns;
            g_prepass_timing.t_submit_ns += probe_submit_ns;
        }

        return ok;
    }

    bool WorldRenderRunner::RenderTo(graph::IRenderTarget *rt, const hgl::Color4f &clear, float deltaTime, CullMode cull_mode)
    {
        RenderPassRequest req;
        req.target           = rt;
        req.clear            = clear;
        req.use_target_clear = false;
        req.delta_time       = deltaTime;
        req.cull_mode        = cull_mode;
        return RenderTo(req);
    }

    void WorldRenderRunner::OnResize(const VkExtent2D &extent)
    {
        if (!context || !context->IsActive())
            return;

        GLogInfo("[WorldRenderRunner] OnResize: %s %ux%u", context->GetName().c_str(), extent.width, extent.height);

        main_frame_topology.MarkTopology(TopologyChange::PassAndTarget);
        main_frame_topology.InvalidateSnapshot();

        auto *target = context->GetRenderTarget();
        if (target)
            target->OnResize(extent);

        auto render_target_system = context->GetSystem<RenderTargetSystem>();
        if (render_target_system)
        {
            render_target_system->SetRenderTarget(target);
        }
        else
        {
            auto camera_system = context->GetSystem<CameraSystem>();
            if (camera_system && target)
                camera_system->SetViewportInfo(target->GetViewportInfo());
        }

        auto camera_system = context->GetSystem<CameraSystem>();
        if (camera_system)
        {
            camera_system->MarkAllCameraMatricesDirty();
            camera_system->ForceRefreshSelectedCamera();
        }
    }

    void WorldRenderRunner::Render(float deltaTime, const std::function<void(float)> &pre_render)
    {
        if (!context)
            return;

        main_frame_topology.BeginFrame(context->GetGraphicsContext() ? context->GetGraphicsContext()->GetGlobalSSBOBufferRegistry() : nullptr);

        if (adaptive_graph_cache.IsDirty())
        {
            SceneStats stats = GatherSceneStats(context);
            const uint64_t current_hash = stats.GetHash();

            if (adaptive_graph_cache.NeedsRebuild(current_hash))
                adaptive_graph_cache.Store(current_hash, CreateAdaptiveRenderGraph(context, stats));

            adaptive_graph_cache.MarkClean();
        }

        context->Render(deltaTime, adaptive_graph_cache.Graph(), pre_render);
    }

    void WorldRenderRunner::RenderPreBeginFrame(float deltaTime)
    {
        if (!context || !context->IsActive())
            return;

        RunRenderPhaseUpdates(ExecutionPhase::RenderPreBeginFrame, deltaTime);
    }

    void WorldRenderRunner::RenderSwapchainNextImage(float deltaTime)
    {
        if (!context || !context->IsActive())
            return;

        RunRenderPhaseUpdates(ExecutionPhase::RenderSwapchainNextImage, deltaTime);
    }

    bool WorldRenderRunner::AcquireSwapchainImage(float deltaTime)
    {
        if (!context || !context->IsActive())
            return false;

        if (!RecreateSwapchainIfNeeded())
        {
            if (auto *target = context->GetRenderTarget())
            {
                auto *swapchain_rt = dynamic_cast<graph::SwapchainRenderTarget *>(target);
                if (swapchain_rt && swapchain_rt->NeedsResize())
                    return false;
            }
        }

        bool swapchain_ok = true;
        bool swapchain_system_present = false;

        RenderSwapchainNextImage(deltaTime);

        if (auto swapchain_system = context->GetSystem<SwapchainNextImageSystem>())
        {
            swapchain_system_present = true;
            swapchain_ok = swapchain_system->WasSuccessful();
        }

        if (!swapchain_system_present)
        {
            if (auto* target = context->GetRenderTarget())
            {
                if (auto swapchain_rt = dynamic_cast<graph::SwapchainRenderTarget*>(target))
                    swapchain_ok = swapchain_rt->NextFrame();
            }
        }

        if (!swapchain_ok)
            RecreateSwapchainIfNeeded();

        return swapchain_ok;
    }

    void WorldRenderRunner::SyncRenderTargetViewportToExtent()
    {
        if (!context || !context->IsActive())
            return;

        auto *target = context->GetRenderTarget();
        if (!target)
            return;

        const VkExtent2D &ext = target->GetExtent();
        const auto *vp_info = target->GetViewportInfo();

        if (vp_info && (vp_info->GetViewport().x != ext.width || vp_info->GetViewport().y != ext.height))
            target->OnResize(ext);
    }

    void WorldRenderRunner::RenderBufferCommit(float deltaTime)
    {
        if (!context || !context->IsActive())
            return;

        RunRenderPhaseUpdates(ExecutionPhase::RenderBufferCommit, deltaTime);
    }

    void WorldRenderRunner::RenderBufferUpload(float deltaTime)
    {
        if (!context || !context->IsActive())
            return;

        RunRenderPhaseUpdates(ExecutionPhase::RenderBufferUpload, deltaTime);
    }

    void WorldRenderRunner::RenderFrameSync(float deltaTime)
    {
        if (!context || !context->IsActive())
            return;

        RunRenderPhaseUpdates(ExecutionPhase::RenderFrameSync, deltaTime);
    }

    void WorldRenderRunner::PrepareRenderPassSetup(uint32_t frameIndex, float deltaTime)
    {
        if (!context || !context->IsActive())
            return;

        SetFrameIndex(frameIndex);
        render_frame_cache.BeginFrame();

        if (auto transform_system = context->GetSystem<TransformSystem>())
        {
            FrameProfileSpan transform_span(FrameProfileSlot::TransformSubmit);
            transform_system->SubmitTransformUpdates();
        }

        if (auto cs = context->GetSystem<CameraSystem>())
        {
            FrameProfileSpan camera_span(FrameProfileSlot::CameraPublish);
            cs->PublishCameraRows(frameIndex);
        }

        {
            FrameProfileSpan collect_span(FrameProfileSlot::CollectPhase);
            RunRenderPhaseUpdates(ExecutionPhase::RenderCollect, deltaTime);
        }

        {
            FrameProfileSpan sync_span(FrameProfileSlot::SyncWorldAddresses);
            context->SyncWorldAddresses();
        }

        {
            FrameProfileSpan batch_span(FrameProfileSlot::BatchPhase);
            RunRenderPhaseUpdates(ExecutionPhase::RenderBatch, deltaTime);
        }

        {
            FrameProfileSpan commit_span(FrameProfileSlot::BufferCommit);
            RenderBufferCommit(deltaTime);
        }

        {
            FrameProfileSpan upload_span(FrameProfileSlot::BufferUpload);
            RenderBufferUpload(deltaTime);
        }

        {
            FrameProfileSpan framesync_span(FrameProfileSlot::FrameSync);
            RenderFrameSync(deltaTime);
        }
    }

    void WorldRenderRunner::RenderSubmit(float deltaTime)
    {
        if (!context || !context->IsActive())
            return;

        RunRenderPhaseUpdates(ExecutionPhase::RenderSubmit, deltaTime);
    }

    bool WorldRenderRunner::SubmitFrameToRenderTarget(float deltaTime)
    {
        if (!context || !context->IsActive())
            return false;

        bool submit_ok = false;
        bool submit_system_present = false;

        RenderSubmit(deltaTime);

        if (auto submit_system = context->GetSystem<SwapchainSubmitSystem>())
        {
            submit_system_present = true;
            submit_ok = submit_system->WasSuccessful();
        }

        if (!submit_system_present)
        {
            if (auto* target = context->GetRenderTarget())
                submit_ok = target->Submit();
        }

        const bool recreated = RecreateSwapchainIfNeeded();
        return submit_ok || recreated;
    }

    bool WorldRenderRunner::RecreateSwapchainIfNeeded()
    {
        if (!context)
            return false;

        auto *target = context->GetRenderTarget();
        auto *swapchain_rt = dynamic_cast<graph::SwapchainRenderTarget *>(target);
        if (!swapchain_rt || !swapchain_rt->NeedsResize() || !context->GetGraphicsContext())
            return false;

        auto *module_manager = context->GetGraphicsContext()->GetModuleManager();
        auto *swapchain_module = module_manager ? module_manager->Get<graph::SwapchainModule>() : nullptr;
        if (!swapchain_module)
            return false;

        VkExtent2D current_extent = swapchain_rt->GetExtent();
        swapchain_module->OnResize(current_extent);
        auto *new_target = swapchain_module->GetRenderTarget();
        context->SetRenderTarget(new_target);
        if (new_target)
        {
            OnResize(new_target->GetExtent());
        }
        return new_target != nullptr;
    }

    void WorldRenderRunner::RunRenderPhaseUpdates(ExecutionPhase phase, float deltaTime)
    {
        if (!context)
            return;

        auto &registry = context->GetSystemRegistry();
        registry.SortRender();

        for (auto& entry : registry.RenderOrder())
        {
            if (!entry.system)
                continue;

            if (entry.phase != static_cast<int>(phase))
                continue;

            RunSystemUpdate(entry.system.get(), deltaTime);
        }
    }

    void WorldRenderRunner::RunRenderPhaseUpdates(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime)
    {
        if (!context)
            return;

        auto &registry = context->GetSystemRegistry();
        registry.SortRender();

        const int min_phase = static_cast<int>(minPhase);
        const int max_phase = static_cast<int>(maxPhase);

        for (auto& entry : registry.RenderOrder())
        {
            if (!entry.system)
                continue;

            if (entry.phase < min_phase || entry.phase > max_phase)
                continue;

            RunSystemUpdate(entry.system.get(), deltaTime);
        }
    }

    void WorldRenderRunner::RunSystemUpdate(System *system, float deltaTime)
    {
        if (!system || !system->IsEnabled())
            return;

        if (FrameProfiler::IsEnabled())
        {
            const uint64_t t0 = GetUptimeUs();

            system->Update(deltaTime);

            AnsiString name = system->GetName();
            if (name.IsEmpty())
                name = AnsiString(typeid(*system).name());

            SystemProfiler::Add(name, system->GetRenderElementType(),
                                static_cast<uint32_t>(system->GetExecutionPhase()),
                                GetUptimeUs() - t0);
        }
        else
        {
            system->Update(deltaTime);
        }
    }

    void WorldRenderRunner::RunRenderSystemsInRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime)
    {
        if (!context)
            return;

        auto &registry = context->GetSystemRegistry();
        registry.SortRender();

        auto *device = context->GetGPUDevice();
        if (device)
            device->SetDrawPhaseActive(true);

        const int min_phase = static_cast<int>(minPhase);
        const int max_phase = static_cast<int>(maxPhase);

        for (auto& entry : registry.RenderOrder())
        {
            if (!entry.system || !entry.system->IsEnabled())
                continue;

            if (entry.phase < min_phase || entry.phase > max_phase)
                continue;

            if (FrameProfiler::IsEnabled())
            {
                const uint64_t t0 = GetUptimeUs();

                entry.system->Render(current_render_cmd, deltaTime);

                AnsiString name = entry.system->GetName();
                if (name.IsEmpty())
                    name = AnsiString(typeid(*entry.system).name());

                SystemProfiler::Add(name, entry.system->GetRenderElementType(),
                                    static_cast<uint32_t>(entry.phase),
                                    GetUptimeUs() - t0);
            }
            else
            {
                entry.system->Render(current_render_cmd, deltaTime);
            }
        }

        if (device)
            device->SetDrawPhaseActive(false);
    }
} // namespace hgl::ecs
