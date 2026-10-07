#include<hgl/ecs/systems/render/MainLightShadowSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/support/CameraInfoStorage.h>
#include<hgl/ecs/support/CameraSlotGuard.h>
#include<hgl/ecs/core/RenderPassRequest.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/render/shadow/CascadedShadowController.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include <cstdlib>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    MainLightShadowSystem::MainLightShadowSystem(const std::string &name)
        : System(name)
        , shadow_controller()
        , shadow_enabled(false)
    {
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);
    }

    MainLightShadowSystem::~MainLightShadowSystem()
    {
        DisableMainLightShadow();
    }

    // T6b/P4：影子数据的 CPU 权威在**世界**（`ECSContext::shadow_info`），不再挂 env profile。
    // 本系统只是它的写者（prepass 里逐帧解算级联），GPU 侧由提交点的
    // `ECSContext::CommitShadowInfo` 写本帧槽。
    graph::ShadowInfo *MainLightShadowSystem::EditShadowInfo()
    {
        return context ? context->EditShadowInfo() : nullptr;
    }

    const graph::ShadowInfo *MainLightShadowSystem::GetShadowInfo() const
    {
        return context ? context->GetShadowInfo() : nullptr;
    }

    void MainLightShadowSystem::SetShadowInfo(const graph::ShadowInfo &info)
    {
        if (context)
            context->SetShadowInfo(info);
    }

    bool MainLightShadowSystem::EnableMainLightShadow(const graph::CascadedShadowConfig &config, uint32_t shadow_map_size)
    {
        if (shadow_enabled)
            DisableMainLightShadow();

        graph::GraphicsContext *gc = nullptr;
        if (render_context)
            gc = render_context->GetGraphicsContext();
        else if (context)
            gc = context->GetGraphicsContext();

        if (!gc)
        {
            GLogError("[MainLightShadowSystem] EnableMainLightShadow failed: GraphicsContext unavailable");
            return false;
        }

        auto *rtm = gc->GetRenderTargetManager();
        auto *btm = gc->GetBindlessTextureManager();
        if (!rtm || !btm)
        {
            GLogError("[MainLightShadowSystem] EnableMainLightShadow failed: RenderTargetManager or BindlessTextureManager unavailable");
            return false;
        }

        shadow_controller = std::make_unique<graph::CascadedShadowController>();
        shadow_controller->SetConfig(config);

        for (uint32_t c = 0; c < graph::kMaxShadowCascades; ++c)
        {
            graph::RenderTargetDesc desc = graph::RenderTargetDesc::OffscreenDepthOnly(
                shadow_map_size, shadow_map_size,
                AnsiString("CSM_Cascade_") + AnsiString::numberOf(c),
                PF_D32F);

            cascade_rts[c] = rtm->Create(desc);
            if (!cascade_rts[c] || !cascade_rts[c]->hasDepth())
            {
                GLogError("[MainLightShadowSystem] EnableMainLightShadow: failed to create depth RT for cascade %u", c);
                DisableMainLightShadow();
                return false;
            }

            auto *depth_tex = cascade_rts[c]->GetDepthTexture();
            if (!depth_tex)
            {
                GLogError("[MainLightShadowSystem] EnableMainLightShadow: depth texture null for cascade %u", c);
                DisableMainLightShadow();
                return false;
            }

            cascade_handles[c] = btm->RegisterTexture(depth_tex);
            shadow_controller->SetCascadeTexture(c, cascade_handles[c], 0);
        }

        // 光相机 = **世界相机表里的一个槽 + 一个宿主**（槽账目的唯一真源 = `CameraInfoStorage`）：
        // 槽由 `light_camera_slot` 这个 RAII guard 持有 —— 构造（Enable）申请、Disable / 本系统
        // 销毁时 Reset 归还，不需要第二套记账、也不用手工 Acquire/Release。
        // 槽耗尽（上限 16、不扩容）时 guard 报错且 invalid ⇒ 相机保持无槽（发布被跳过 + 一次性告警），
        // 而不是按越界行号写到别的世界去。
        light_camera = std::make_shared<CameraComponent>("AutoCSMLightCamera");
        light_camera->is_main_camera = false;

        light_camera_slot = CameraSlotGuard(context, light_camera, "AutoCSMLightCamera");

        // A3：以 Enable 时刻的 revision 为消费基线，避免启用后第一帧立即
        // 多做一次静态级联全量重建。
        consumed_static_scene_revision = context ? context->GetStaticSceneRevision() : 0;

        // 影随光（T6b）：在主光行上落「投阴影 + 机制 = CSM + 资源索引 0（本世界唯一一份影子数据）」。
        // 光行是影子的**归属方**（阴影从属于光源），本系统只是机制的执行者。
        if (context)
        {
            if (auto *row = context->GetLightRowTable().Row(graph::kMainLightRowIndex))
            {
                graph::LightRow light = *row;

                light.flags       |= graph::kLightFlagCastShadow;
                light.shadow_kind  = uint32_t(graph::LightShadowKind::CSM);
                light.shadow_index = 0;

                context->GetLightRowTable().SetRow(graph::kMainLightRowIndex, light);
            }
        }

        disabled_light_reported = false;

        shadow_enabled = true;
        GLogInfo("[MainLightShadowSystem] Main light shadow enabled successfully (4 cascades, size=%u)", shadow_map_size);
        return true;
    }

    void MainLightShadowSystem::DisableMainLightShadow()
    {
        for (uint32_t c = 0; c < graph::kMaxShadowCascades; ++c)
        {
            cascade_rts[c].reset();
            cascade_handles[c] = 0;
            cascade_depth_range[c] = 0.0f;
        }

        shadow_controller.reset();

        // 归还光相机占的**本世界**相机槽：槽由 guard 持有 ⇒ Reset 即对称归还（账目里那一条随之清空）。
        // 世界槽上限 16、超限报错不扩容：每次 Enable/Disable 泄漏一槽迟早把槽位顶满。
        light_camera_slot.Reset();
        light_camera.reset();

        // 影随光（T6b）：光行上的阴影开关随机制一起清（不投阴影 / 机制 None / 索引归零）
        if (context)
        {
            if (auto *row = context->GetLightRowTable().Row(graph::kMainLightRowIndex))
            {
                graph::LightRow light = *row;

                light.flags       &= ~graph::kLightFlagCastShadow;
                light.shadow_kind  = uint32_t(graph::LightShadowKind::None);
                light.shadow_index = 0;

                context->GetLightRowTable().SetRow(graph::kMainLightRowIndex, light);
            }
        }

        shadow_enabled = false;
    }

    graph::IRenderTarget *MainLightShadowSystem::GetCascadeRenderTarget(uint32_t cascade_index) const
    {
        if (cascade_index < graph::kMaxShadowCascades)
            return cascade_rts[cascade_index].get();
        return nullptr;
    }

    void MainLightShadowSystem::InvalidateMainLightStaticShadowCache()
    {
        // 双通道失效：控制器缓存立即作废（下帧 Update 全量重建），revision
        // 消费点前移防止随后到达的 TransformSystem 旧信号重复触发。
        // shadow_enabled == false 时也递增 context revision：Enable 的基线
        // 取的是"当时"值，提前 bump 的变更信号会在启用后被消费。
        if (shadow_controller)
            shadow_controller->InvalidateStaticCache();

        if (context)
        {
            context->BumpStaticSceneRevision();
            consumed_static_scene_revision = context->GetStaticSceneRevision();
        }
    }

    void MainLightShadowSystem::RenderMainLightShadowPass(CameraComponent *main_camera, float deltaTime)
    {
        if (!shadow_enabled || !shadow_controller || !context || !main_camera || !light_camera)
            return;

        auto *shadow_info = EditShadowInfo();
        if (!shadow_info)
            return;

        // 光源定义 = **本世界的光源行 0**（T6b；写者是 `LightSystem`，本系统只读 —— 阴影从属于
        // 光源、单向依赖）。不经 `GetSystem<LightSystem>()` 取，避免反向依赖：行表归世界所有。
        const auto *light_row = context->GetLightRowTable().Row(graph::kMainLightRowIndex);
        if (!light_row || (light_row->flags & graph::kLightFlagEnabled) == 0)
        {
            if (!disabled_light_reported)
            {
                disabled_light_reported = true;
                GLogWarning("[MainLightShadowSystem] 主方向光未启用（光行 0 的 flags 缺 enabled）"
                            "⇒ 本帧跳过阴影解算；请经 LightSystem 点亮主光");
            }

            return;
        }

        // 光行约定：direction 指向**光源**；CSM 解算要的是「光源到表面」的方向 ⇒ 取反
        const auto &to_light = light_row->direction_intensity;
        math::Vector3f light_dir_math(-to_light.x, -to_light.y, -to_light.z);

        // 主相机当前视锥数据
        graph::Camera main_cam;
        main_cam.pos = main_camera->position;
        main_cam.viewDirection = main_camera->forward;
        main_cam.world_up = main_camera->world_up;
        main_cam.fovY = main_camera->fov > 0.0f ? main_camera->fov : 60.0f;
        main_cam.znear = main_camera->near_plane > 0.0f ? main_camera->near_plane : 0.1f;
        main_cam.zfar = main_camera->far_plane > 0.0f ? main_camera->far_plane : 500.0f;

        const auto *vp = main_camera->viewport_info;
        const float aspect = (vp && vp->GetViewportHeight() > 0)
            ? vp->GetAspectRatio()
            : (16.0f / 9.0f);

        // A3：TransformSystem 在 TickTransform 检出 Static transform 变更时
        // 递增 static_scene_revision（含位置/旋转/缩放/父子/Mobility）。此处
        // 比对消费：revision 前移即失效静态级联缓存，当帧 prepass 全量重建，
        // 随后追平不再触发。static 级联的"静态"前提由该钩子兜底——否则
        // 相机静止时缓存的旧深度永不刷新。
        if (context)
        {
            const uint64_t revision = context->GetStaticSceneRevision();
            if (revision != consumed_static_scene_revision)
            {
                shadow_controller->InvalidateStaticCache();
                consumed_static_scene_revision = revision;
                GLogInfo("[MainLightShadowSystem] static scene revision %llu -> invalidating static cascade cache",
                         static_cast<unsigned long long>(revision));
            }
        }

        graph::CascadeUpdateResult updates[graph::kMaxShadowCascades];
        shadow_controller->Update(main_cam, aspect, light_dir_math, *shadow_info, updates);

        for (uint32_t c = 0; c < graph::kMaxShadowCascades; ++c)
            cascade_depth_range[c] = updates[c].depth_range;

        // 级联屏蔽处理：若某级被关闭，将 shadow_tex 置 0，使 Shader 判定该级全受光（无阴影）
        uint32_t active_global_tex = 0;
        for (uint32_t c = 0; c < graph::kMaxShadowCascades; ++c)
        {
            if (!IsCascadeEnabled(c))
            {
                shadow_info->cascades[c].shadow_tex = math::Vector4u(0);
            }
            else if (active_global_tex == 0)
            {
                active_global_tex = cascade_handles[c];
            }
        }

        shadow_info->shadow_tex.x = active_global_tex;

        for (uint32_t c = 0; c < graph::kMaxShadowCascades; ++c)
        {
            if (!IsCascadeEnabled(c))
                continue; // 该级联被关闭，跳过渲染

            const auto &res = updates[c];
            auto *rt = cascade_rts[c].get();
            if (!rt)
                continue;

            // S6 诊断开关（CSM_PASS_LOG=1，默认静默）：逐 pass 打印级联/类型/scissor/写侧平移，
            // 与 [S6-COLLECT] 的"图元数 + 实体 ID 校验和"配对读：同批 caster ⇒ 差异在光栅化侧；
            // 不同批 ⇒ 差异在收集/剔除侧。
            static const bool s6_pass_log = (std::getenv("CSM_PASS_LOG") != nullptr);

            if (c == 0 || res.need_full_update)
            {
                RenderPassRequest req;
                req.target = rt;
                req.camera = light_camera.get();
                light_camera->custom_matrices = true;
                // 写侧用 light_view_draw：非零环形偏移时它把内容光栅化到物理贴图坐标系，
                // 与读侧 shader 的 fract(shadow_uv + cache_offset·texel) 对齐（偏移为 0 时
                // 它与 light_view 逐位相同）。
                light_camera->custom_view = res.light_view_draw;
                light_camera->custom_projection = res.light_proj;
                req.load_depth = false;
                req.use_scissor = false;
                req.clear_scissor_depth = false;
                req.cull_mode = CullMode::Front;
                req.is_shadow_pass = true;
                req.shadow_reference_camera = main_camera;
                req.mobility_filter = (c == 0) ? static_cast<int>(Mobility::Movable) : static_cast<int>(Mobility::Static);

                context->RenderTo(req);

                if (s6_pass_log)
                    GLogInfo("[S6-PASS] cascade=%u kind=full mobility=%d off=(%u,%u) "
                             "view_t=(%.4f,%.4f) load_depth=%d",
                             c, req.mobility_filter, res.cache_offset.x, res.cache_offset.y,
                             res.light_view_draw[3][0], res.light_view_draw[3][1],
                             req.load_depth ? 1 : 0);
            }
            else if (res.dirty_rect_count > 0)
            {
                for (uint32_t r = 0; r < res.dirty_rect_count; ++r)
                {
                    const auto &rect = res.dirty_rects[r];
                    RenderPassRequest req;
                    req.target = rt;
                    req.camera = light_camera.get();
                    light_camera->custom_matrices = true;
                    // 局部条带重画：dirty_rects 已是**物理**坐标（含环形接缝拆分），
                    // 用写侧矩阵落在同一坐标系；load_depth=true 保留其余缓存内容。
                    light_camera->custom_view = res.light_view_draw;
                    light_camera->custom_projection = res.light_proj;
                    req.load_depth = true;
                    req.use_scissor = true;
                    req.scissor.offset = { static_cast<int32_t>(rect.x), static_cast<int32_t>(rect.y) };
                    req.scissor.extent = { rect.width, rect.height };
                    req.clear_scissor_depth = true;
                    req.cull_mode = CullMode::Front;
                    req.is_shadow_pass = true;
                    req.shadow_reference_camera = main_camera;
                    req.mobility_filter = static_cast<int>(Mobility::Static);

                    context->RenderTo(req);

                    if (s6_pass_log)
                        GLogInfo("[S6-PASS] cascade=%u kind=strip mobility=%d off=(%u,%u) "
                                 "view_t=(%.4f,%.4f) rect=(%u,%u,%u,%u) load_depth=%d",
                                 c, req.mobility_filter, res.cache_offset.x, res.cache_offset.y,
                                 res.light_view_draw[3][0], res.light_view_draw[3][1],
                                 rect.x, rect.y, rect.width, rect.height,
                                 req.load_depth ? 1 : 0);
                }
            }
        }
    }
}//namespace hgl::ecs
