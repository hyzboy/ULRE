#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/core/RenderPassRequest.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/render/lighting/CascadedShadowController.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    EnvironmentSystem::EnvironmentSystem(const std::string &name)
        : System(name)
        , shadow_controller(nullptr)
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

        shadow_controller = new graph::CascadedShadowController();
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
        }

        light_camera = std::make_shared<CameraComponent>("AutoCSMLightCamera");
        light_camera->is_main_camera = false;

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

        if (shadow_controller)
        {
            delete shadow_controller;
            shadow_controller = nullptr;
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

        graph::CascadeUpdateResult updates[graph::kMaxShadowCascades];
        shadow_controller->Update(main_cam, aspect, light_dir_math, *shadow_info, updates);

        for (uint32_t c = 0; c < graph::kMaxShadowCascades; ++c)
            cascade_depth_range[c] = updates[c].depth_range;

        shadow_info->shadow_tex.x = cascade_handles[0];
        MarkShadowDirty();

        for (uint32_t c = 0; c < graph::kMaxShadowCascades; ++c)
        {
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
                light_camera->custom_view = res.light_view;
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
                    light_camera->custom_view = res.light_view;
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
