#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/systems/render/RenderSystemCore.h>
#include <hgl/ecs/systems/render/RenderTargetSystem.h>
#include <hgl/ecs/systems/render/MainLightShadowSystem.h>
#include <hgl/ecs/systems/render/SwapchainNextImageSystem.h>
#include <hgl/ecs/systems/render/SwapchainSubmitSystem.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/tick/TransformSystem.h>
#include <hgl/ecs/support/CameraInfoStorage.h>
#include <hgl/ecs/support/PrimitiveState.h>
#include <hgl/ecs/components/GeometryData.h>
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
#include <cstdlib>
#include <cstdio>
#include <vector>

namespace hgl::ecs
{
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

        // ── T8（D7）prepass 性能账目：`ULRE_DEBUG.toml` 的 `channels.perf = true`
        //（`[perf] prepass_interval` 可指定打印间隔；关闭时 = 完全零成本）──
        // 用途 = 量**CPU 可见账**：每次离屏 pass 的墙钟总耗时 = 阻塞（上沿 `saved_target->WaitFence()`
        // 等主帧在途槽排空 + 下沿 `rt->WaitFence()` 等本次离屏 RT 自己的 fence）+ 非阻塞
        //（录制 / 提交 / 相机与槽簿记）。本引擎**没有 GPU 时间戳设施**（全仓无 `vkCmdWriteTimestamp` /
        // `VkQueryPool`）⇒ 只能答"CPU 被什么卡住"，答不了"GPU 用了多久"；要 GPU 侧时间须另立设施。
        // 正式基线见 `doc/csm-mechanism.md` §6（T8 `f8760056b`：稳态 **1.00 离屏 pass/帧**、3.76 ms/帧 ≈ 主帧 19%，
        // 其中 `begin` 2.32 ms 是大头、下沿 0.23 ms）。
        // ⚠ 两点别记错：① **CSM 稳态只有级联 0 每帧全量更新**（其余 3 级 = 静态滚动缓存 + 条带脏矩形）
        //   ⇒ 不存在"每帧 4 次离屏提交"；② 「4 级联合并成单 cmd / 单 prepass」（= A6）**已被否决**
        //   （`doc/csm-mechanism.md` §6；`doc/backlog.md` 顶部「已否决」表）⇒ 别拿这些数字去论证它。
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
                uint32_t frames       = 0;   // 本窗口主帧数
                uint64_t t_frame_ns   = 0;   // 本窗口主帧间距之和
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

                // 打印间隔来自 ULRE_DEBUG.toml 的 [perf] prepass_interval（缺省 60 帧）
                const int64_t interval =
                    hgl::debug::ConfigInt("perf", "prepass_interval", 60);

                if (interval > 0 && interval <= 0xFFFF)
                    g_prepass_timing.print_interval = static_cast<uint32_t>(interval);

                GLogInfo("[PREPASS-TIMING] 启用（ULRE_DEBUG.toml 的 channels.perf=true，"
                         "间隔 [perf] prepass_interval=%u）：每 %u 个主帧报一次均值；"
                         "本引擎无 GPU 时间戳 ⇒ 量的是 CPU 可见账（两处 fence 等待 + 其余）",
                         g_prepass_timing.print_interval, g_prepass_timing.print_interval);
            }

            // 主帧入口调用：闭合上一帧、按窗口打印均值并复位
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
        }

        bool ECSContext::HasPendingRenderableResources() const
        {
            ValueArray<GeometryData *> primitives;
            GetComponents<GeometryData>(primitives);

            for (GeometryData *geometry : primitives)
            {
                if (!geometry)
                    continue;

                if (HasPendingMaterialResources(geometry->GetOwner()))
                    return true;
            }

            return false;
        }

        void ECSContext::ExecuteScenePrePassWorkflow(float deltaTime)
        {
            switch (scene_pipeline_mode)
            {
                case ScenePipelineMode::StandardLitCSM:
                {
                    // ── 黄金路径：标准 3D / FPS / TPS 陆地场景主光级联阴影自动化 ──
                    // 阴影托管域 = MainLightShadowSystem（T6a 自 EnvironmentSystem 拆出）
                    if (auto shadow = GetSystem<MainLightShadowSystem>())
                    {
                        if (shadow->IsMainLightShadowEnabled())
                        {
                            if (auto cam_sys = GetSystem<CameraSystem>())
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

            // T8：主帧边界 = prepass 性能账目的帧界（打印上一窗口均值 + 复位）
            if (need_swapchain_acquire)
                PrepassTimingFrameBoundary();

            // ── D6：场景预处理（主光 CSM 阴影）挂到**帧入口**，不再靠调用方记得调用 ──
            // 历史形态只有 RenderGraph 那条 Render() 显式调用它（单入口）⇒ 第二个帧入口
            // 一出现就是"阴影静默失效、断言与日志都没有"（离屏世界正是第二个入口）。
            // 判别式 = 「主帧」：离屏帧一律传 need_swapchain_acquire=false（RenderTo 路径），
            // 其嵌套的阴影 pass 因此也不会递归进来。
            // 位置保持 AcquireSwapchainImage 之前 = 黄金路径原序（先渲阴影 RT，主帧再取图）。
            if (need_swapchain_acquire)
                ExecuteScenePrePassWorkflow(deltaTime);

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
            SyncRenderTargetViewportToExtent();

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

            // T8：本次离屏 pass 的墙钟账（未启用 perf 通道时 probe_t0 = 0，后续判断全短路）
            PrepassTimingInit();
            const uint64_t probe_t0 = g_prepass_timing.enabled ? PrepassTimingNow() : 0;
            uint64_t probe_up_ns = 0;
            uint64_t probe_down_ns = 0;
            uint64_t probe_begin_ns = 0;    // BeginManagedRenderFrame（含 PrepareRenderPassSetup：collect/批装配/剔除派发）
            uint64_t probe_draw_ns = 0;     // RenderDrawOnly（录制绘制）
            uint64_t probe_submit_ns = 0;   // EndManagedRenderFrame（提交 + 呈现）

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

                        // 正向判据（一次性）：确认"A1 车道等待"真的挂上了——
                        // 此前车道恒 null（构造期设备未绑定），这段被静默跳过。
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

            // 共享 per-frame 资源保护（上沿）：Camera UBO 与 L2W ring 为单份内存，
            // 覆写前等在途主帧 GPU 完成，防止其 draw 读到覆盖后的数据
            const uint64_t probe_up_begin = (probe_t0 != 0) ? PrepassTimingNow() : 0;
            if (saved_target)
                saved_target->WaitFence();
            if (probe_up_begin != 0)
                probe_up_ns = PrepassTimingNow() - probe_up_begin;

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

            // RAII 状态恢复守卫：保证提前 return 或异常退出时，全套全局状态无条件还原
            struct RenderToStateGuard
            {
                ECSContext* ctx;
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
                        ctx->render_target = saved_target;
                        ctx->SetFrameIndex(saved_target ? saved_target->GetCurrentFrameIndex() : 0u);

                        if (restore_clear && current_rt)
                            current_rt->SetClearColor(saved_clear);

                        if (rts)
                            rts->SetRenderTarget(rts_saved ? rts_saved : saved_target);

                        if (use_override && cs)
                            cs->RestoreMainCamera();

                        ctx->active_camera_id = CameraInfoStorage::kDefaultCameraSlot;
                        ctx->active_mobility_filter = -1;
                        ctx->is_current_pass_shadow = false;
                        ctx->has_shadow_origin = false;
                    }
                }
            } state_guard{
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
            // pass 身份**由目标相对本世界主渲染目标推导**，不再只依赖调用方声明。
            // 判据不是"离屏与否"，而是"**是否本世界的主目标**"：
            //   · `req.target == render_target` ⇒ 这就是本世界的正常 forward pass——
            //     **独立离屏世界（OffscreenWorld）的主目标是自己的 RT**，它必须走完整
            //     forward 收集；按"非交换链 ⇒ shadow"误判会让 collect 只收 ShadowCaster
            //     ⇒ 子世界无 caster ⇒ 收集为空 ⇒ 离屏 cmd 里只有 Begin/EndRendering
            //     （实测 RenderToTexture / RenderToTextureColorDepth 离屏全空）。
            //   · `req.target != render_target`（同一世界渲进 shadow/mirror 等**别的**目标）
            //     ⇒ 与主帧**共享批对象/GPU 接管资源**，必须错开 Main 块——这才是第一例
            //     （ShadowMap/ATS 漏设旗标 ⇒ 改写共享 `gpu_slice[Main]` ⇒ 某张交换链图整帧空）
            //     的病源，漏设旗标仍被本推导兜住。
            // 两个正交条件都保留：`req.is_shadow_pass`（调用方显式声明）与"异目标"推导。
            // ⚠ 必须比 **saved_target 快照**：:638 已把 `render_target` 切成本 pass 的目标，
            //   此刻再读 `render_target` 恒等于 req.target ⇒ 判据永假、兜底失效（实测踩过）。
            const bool foreign_target =
                req.target && (req.target != saved_target);

            is_current_pass_shadow = req.is_shadow_pass || foreign_target;
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

            const uint64_t probe_begin_start = (probe_t0 != 0) ? PrepassTimingNow() : 0;

            // 离屏 RT 无 swapchain 图像可获取，跳过 AcquireSwapchainImage
            if (BeginManagedRenderFrame(req.delta_time, false, p_options))
            {
                if (probe_begin_start != 0)
                    probe_begin_ns = PrepassTimingNow() - probe_begin_start;

                // 本 pass 的 per-frame 数据槽 = 该 RT 的槽带（与主帧槽不相交）
                SetFrameIndex(render_target ? render_target->GetCurrentFrameIndex() : 0u);

                // 本次 pass 的相机行：**按 req.camera 直接发布**。阴影光源相机是**系统内建相机**
                // （EnvironmentSystem 直接 make_shared，不经 Entity/AddComponent 注册）⇒ 不在
                // 实体组件存储里，本世界 CollectCameras() 看不到它，靠通用发布会漏 ⇒
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

                const uint64_t probe_draw_start = (probe_t0 != 0) ? PrepassTimingNow() : 0;
                RenderDrawOnly(render_core->GetRenderCmd(), req.delta_time);
                if (probe_draw_start != 0)
                    probe_draw_ns = PrepassTimingNow() - probe_draw_start;

                const uint64_t probe_submit_start = (probe_t0 != 0) ? PrepassTimingNow() : 0;
                EndManagedRenderFrame(req.delta_time);
                if (probe_submit_start != 0)
                    probe_submit_ns = PrepassTimingNow() - probe_submit_start;

                ok = true;
            }

            active_mobility_filter = -1;
            is_current_pass_shadow = false;
            has_shadow_origin = false;

            // 关键同步（下沿保护）：本帧离屏提交完成后立即等该 RT 自己的 queue fence！
            // 必须在恢复主相机共享数据前等，因为 GPU 仍在异步读取本 pass 提交的光源
            // CameraUBO；若未等即在 CPU 上 Update(0.0f) 覆盖主相机数据，会踩踏 GPU 正在读取的内存，
            // 导致 shadow map 被画成主相机视角（间歇性阴影闪烁丢失）。
            const uint64_t probe_down_begin = (probe_t0 != 0) ? PrepassTimingNow() : 0;
            if (ok)
                rt->WaitFence();
            if (probe_down_begin != 0)
                probe_down_ns = PrepassTimingNow() - probe_down_begin;

            // A1：本帧离屏 RT 的车道值累积给主帧提交等（RenderTargetData::Submit 已 signal）
            if (ok)
                AddFrameLaneWait(rt->GetLane(), rt->GetLaneValue());

            SetSubmitWaitsFromFrameLanes();

            // （render_target / frame_index / clearColor / RenderTargetSystem / Camera 均由 state_guard 析构安全恢复）

            // T8：入账（总耗时含两处等待；非阻塞 = 总 − 等待 = 录制/提交/相机与槽簿记）
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

        /// §2 #4：原名 `SyncRenderTargetViewport` 名不符实（它**不**同步 viewport 尺寸给 Rt，
        /// 而是反过来：把渲染目标的视口同步到该目标当前的 extent）。改名以暴露真实语义，行为不变。
        void ECSContext::SyncRenderTargetViewportToExtent()
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

            // ⚠ 变换提交必须在本函数最前段（SetFrameIndex 之后——L2W ring 帧索引刚被推到位；
            // collect/派发之前——见下）。`SubmitTransformUpdates` 是 **L2W 矩阵行**与实体行
            // `row[Transform]` 的唯一落盘点（WriteEntityTransformRows，TransformSystem.cpp），
            // 而本帧的 GPU 剔除派发在 RenderFrameSync（本函数末尾）就读这两样。
            // 历史上它只挂在 RecordPreparedRenderPhaseRange（BeginRenderPass 之后的绘制录制段）
            // ⇒ 派发恒读到**上一帧**的写入：每帧驱动的世界第 2 帧自愈（稳态滞后一帧），
            // 而 RenderOnce 的离屏世界只有一帧 ⇒ `row[Transform]=0` ⇒ cull 着色器在视锥测试前
            // return ⇒ DrawCount=0、离屏永久空白（实测 RenderToTexture/ColorDepth；
            // 离屏单帧世界缺陷族的第五层（全族五层见 doc/future/entity-rows-gpu-cull-plan.md §11.17）。
            // 同帧幂等（值变才标脏 + 版本记账）⇒ 绘制录制段的第二次调用退化为 no-op。
            if (auto transform_system = GetSystem<TransformSystem>())
                transform_system->SubmitTransformUpdates();   // M8-needle：本调用必须在 collect/派发之前（Test 42 钉住）

            // 相机行发布必须在这之后：本帧数据槽（frameIndex）此时才确定
            // （主帧 = acquire 之后拿到的交换链槽；离屏 pass = 该 RT 的槽带）。
            // 这里只覆盖本世界 CameraSystem 收集得到的相机（= 经 Entity 注册的那些）；系统内建
            // 相机（阴影光源相机等）不在其中，由 RenderTo 按 req.camera 单独发布（见下）。
            if (auto cs = GetSystem<CameraSystem>())
                cs->PublishCameraRows(frameIndex);

            RunRenderPhaseUpdates(ExecutionPhase::RenderCollect,     deltaTime); // collect / cull visible components

            // 实体行表 / 4-ID 表的脏行**由本帧 collect 标出**：帧首那次同步（SetFrameIndex）
            // 只能传到"上一帧的脏行"⇒ 新世界首帧的派发（RenderFrameSync）会读到全零行
            // （Active=0 ⇒ 视锥测试前 return ⇒ DrawCount=0）。每帧驱动的世界第二帧自愈，
            // **只渲染一次的离屏世界（RenderTo 的 RenderOnce 用法）永远等不到那第二帧**
            //（实测 RenderToTexture/ColorDepth 离屏只剩 Begin/End）。⇒ collect 后再同步一次
            //（WA 行 memcmp 未变则不提交；脏行上传只传本帧新标的 ⇒ 二次调用近零成本），
            // 让本帧派发读到**本帧**的行。
            SyncWorldAddresses();

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

//                LogDebug("[ECS] Render Begin: %s (phase %d)", entry.system->GetName().c_str(), entry.phase);

                entry.system->Render(current_render_cmd, deltaTime);

//                LogDebug("[ECS] Render End: %s", entry.system->GetName().c_str());
            }

            if (gpu_device)
                gpu_device->SetDrawPhaseActive(false);
        }
} // namespace hgl::ecs