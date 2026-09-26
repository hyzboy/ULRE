#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/core/RenderPassRequest.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/render/lighting/CascadedShadowController.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    EnvironmentSystem::EnvironmentSystem(const std::string &name)
        : System(name)
        , shadow_controller()
        , shadow_enabled(false)
    {
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);
    }

    EnvironmentSystem::~EnvironmentSystem()
    {
        DisableMainLightShadow();
    }

    graph::EnvProfileID EnvironmentSystem::ResolveProfileID() const
    {
        if (context)
        {
            if (auto *rt = context->GetRenderTarget())
                return rt->GetEnvironmentProfile();
        }

        return graph::kEnvProfileDefault;
    }

    graph::EnvironmentManager *EnvironmentSystem::ResolveManager()
    {
        if (render_context)
        {
            if (auto *gc = render_context->GetGraphicsContext())
                return gc->GetEnvironmentManager();
        }

        if (context)
        {
            if (auto *rc = context->GetRenderContext())
            {
                if (auto *gc = rc->GetGraphicsContext())
                    return gc->GetEnvironmentManager();
            }

            if (auto *gc = context->GetGraphicsContext())
                return gc->GetEnvironmentManager();
        }

        return nullptr;
    }

    graph::SkyInfo *EnvironmentSystem::EditSkyInfo()
    {
        auto *manager = ResolveManager();
        if (!manager)
            return nullptr;

        auto *info = manager->Edit(ResolveProfileID());
        if (!info)
            return nullptr;

        return &info->sky;
    }

    const graph::SkyInfo *EnvironmentSystem::GetSkyInfo() const
    {
        // Edit 路径非 const（manager 懒物化），GetSkyInfo 只读 CPU 数据，
        // 这里的 const_cast 不触发任何 GPU 操作。
        auto *manager = const_cast<EnvironmentSystem *>(this)->ResolveManager();
        if (!manager)
            return nullptr;

        const auto *info = manager->Get(ResolveProfileID());
        if (!info)
            return nullptr;

        return &info->sky;
    }

    void EnvironmentSystem::SetSkyInfo(const graph::SkyInfo &info, bool immediate)
    {
        auto *manager = ResolveManager();
        if (!manager)
            return;

        auto *env = manager->Edit(ResolveProfileID());
        if (!env)
            return;

        env->sky = info;
        if (immediate)
            manager->MarkDirty(ResolveProfileID());
    }

    void EnvironmentSystem::MarkSkyDirty()
    {
        if (auto *manager = ResolveManager())
            manager->MarkDirty(ResolveProfileID());
    }

    graph::ShadowInfo *EnvironmentSystem::EditShadowInfo()
    {
        auto *manager = ResolveManager();
        if (!manager)
            return nullptr;

        auto *info = manager->Edit(ResolveProfileID());
        if (!info)
            return nullptr;

        return &info->shadow;
    }

    const graph::ShadowInfo *EnvironmentSystem::GetShadowInfo() const
    {
        auto *manager = const_cast<EnvironmentSystem *>(this)->ResolveManager();
        if (!manager)
            return nullptr;

        const auto *info = manager->Get(ResolveProfileID());
        if (!info)
            return nullptr;

        return &info->shadow;
    }

    void EnvironmentSystem::SetShadowInfo(const graph::ShadowInfo &info, bool immediate)
    {
        auto *manager = ResolveManager();
        if (!manager)
            return;

        auto *env = manager->Edit(ResolveProfileID());
        if (!env)
            return;

        env->shadow = info;
        if (immediate)
            manager->MarkDirty(ResolveProfileID());
    }

    void EnvironmentSystem::MarkShadowDirty()
    {
        if (auto *manager = ResolveManager())
            manager->MarkDirty(ResolveProfileID());
    }

    bool EnvironmentSystem::EnableMainLightShadow(const graph::CascadedShadowConfig &config, uint32_t shadow_map_size)
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
            GLogError("[EnvironmentSystem] EnableMainLightShadow failed: GraphicsContext unavailable");
            return false;
        }

        auto *rtm = gc->GetRenderTargetManager();
        auto *btm = gc->GetBindlessTextureManager();
        if (!rtm || !btm)
        {
            GLogError("[EnvironmentSystem] EnableMainLightShadow failed: RenderTargetManager or BindlessTextureManager unavailable");
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
                GLogError("[EnvironmentSystem] EnableMainLightShadow: failed to create depth RT for cascade %u", c);
                DisableMainLightShadow();
                return false;
            }

            auto *depth_tex = cascade_rts[c]->GetDepthTexture();
            if (!depth_tex)
            {
                GLogError("[EnvironmentSystem] EnableMainLightShadow: depth texture null for cascade %u", c);
                DisableMainLightShadow();
                return false;
            }

            cascade_handles[c] = btm->RegisterTexture(depth_tex);
            shadow_controller->SetCascadeTexture(c, cascade_handles[c], 0);
        }

        light_camera = std::make_shared<CameraComponent>("AutoCSMLightCamera");
        light_camera->is_main_camera = false;

        // A3：以 Enable 时刻的 revision 为消费基线，避免启用后第一帧立即
        // 多做一次静态级联全量重建。
        consumed_static_scene_revision = context ? context->GetStaticSceneRevision() : 0;

        shadow_enabled = true;
        GLogInfo("[EnvironmentSystem] Main light shadow enabled successfully (4 cascades, size=%u)", shadow_map_size);
        return true;
    }

    void EnvironmentSystem::DisableMainLightShadow()
    {
        for (uint32_t c = 0; c < graph::kMaxShadowCascades; ++c)
        {
            cascade_rts[c].reset();
            cascade_handles[c] = 0;
            cascade_depth_range[c] = 0.0f;
        }

        shadow_controller.reset();

        // A2：光相机经 RenderTo→SetOverrideCamera→BindCameraResources 从
        // GlobalSSBOBufferRegistry 占了一行 CameraInfo（AcquireCamera）。
        // Disable 必须对称归还——registry 容量不预留、超限 fail-fast，
        // 每次 Enable/Disable 泄漏一行迟早把行池顶满。camera_id==0 是
        // "未分配"哨兵（主相机固定占 0 行），不可误归还。
        if (light_camera && light_camera->camera_id != 0)
        {
            graph::GraphicsContext *gc = render_context
                                             ? render_context->GetGraphicsContext()
                                             : (context ? context->GetGraphicsContext() : nullptr);
            if (auto *registry = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr)
            {
                if (!registry->ReleaseCamera(light_camera->camera_id))
                    GLogWarning("[EnvironmentSystem] DisableMainLightShadow: ReleaseCamera(%u) failed (row already freed?)",
                                light_camera->camera_id);
            }
        }

        light_camera.reset();
        shadow_enabled = false;
    }

    graph::IRenderTarget *EnvironmentSystem::GetCascadeRenderTarget(uint32_t cascade_index) const
    {
        if (cascade_index < graph::kMaxShadowCascades)
            return cascade_rts[cascade_index].get();
        return nullptr;
    }

    void EnvironmentSystem::InvalidateMainLightStaticShadowCache()
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

    void EnvironmentSystem::RenderMainLightShadowPass(CameraComponent *main_camera, float deltaTime)
    {
        if (!shadow_enabled || !shadow_controller || !context || !main_camera || !light_camera)
            return;

        auto *shadow_info = EditShadowInfo();
        if (!shadow_info)
            return;

        const auto *sky = GetSkyInfo();
        if (!sky)
            return;

        // 太阳光方向从 sky 提取（指向场景地面的相反方向 = 光源到表面）
        const auto &sun = sky->sun_direction;
        math::Vector3f light_dir_math(-sun.x, -sun.y, -sun.z);

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
                GLogInfo("[EnvironmentSystem] static scene revision %llu -> invalidating static cascade cache",
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
        MarkShadowDirty();

        for (uint32_t c = 0; c < graph::kMaxShadowCascades; ++c)
        {
            if (!IsCascadeEnabled(c))
                continue; // 该级联被关闭，跳过渲染

            const auto &res = updates[c];
            auto *rt = cascade_rts[c].get();
            if (!rt)
                continue;

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
                }
            }
        }
    }
}//namespace hgl::ecs
