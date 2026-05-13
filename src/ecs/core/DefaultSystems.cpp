#include <hgl/ecs/core/DefaultSystems.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/SystemGroup.h>
#include <hgl/ecs/systems/tick/InputSystem.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/tick/LineBoundsUpdateSystem.h>
#include <hgl/ecs/support/text/TextRenderPipelineGroup.h>
#include <hgl/ecs/support/line/LineRenderPipelineGroup.h>
#include <hgl/ecs/support/line/LineCollectSystem.h>
#include <hgl/ecs/support/line/LineRenderSystem.h>
#include <hgl/ecs/support/billboard/BillboardRenderPipelineGroup.h>
#include <hgl/ecs/systems/render/LineStatsSystem.h>
#include <hgl/ecs/systems/render/EnvironmentSystem.h>
#include <hgl/ecs/systems/render/RenderTargetSystem.h>
#include <hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include <hgl/ecs/systems/render/MaterialResolveSystem.h>
#include <hgl/ecs/systems/render/PrimitiveBindingCommitSystem.h>
#include <hgl/ecs/systems/render/TextureMaterialBindingSystem.h>
#include <hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include <hgl/ecs/systems/render/QuadMeshPrepareSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipelineGroup.h>
#include <hgl/ecs/support/terrain/TerrainRenderPipelineGroup.h>
#include <hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include <hgl/ecs/systems/render/SwapchainNextImageSystem.h>
#include <hgl/ecs/systems/render/SwapchainSubmitSystem.h>
#include <hgl/ecs/systems/render/RenderFrameBusinessSyncSystem.h>
#include <hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include <hgl/ecs/systems/render/AssetInstanceCollectSystem.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/vk/VKRenderTarget.h>

namespace
{
    template<typename T>
    std::shared_ptr<T> EnsureTickSystem(hgl::ecs::ECSContext* ctx)
    {
        if (!ctx)
            return nullptr;

        auto system = ctx->GetSystem<T>();
        if (!system)
            system = ctx->RegisterTickSystem<T>();

        return system;
    }

    template<typename T>
    std::shared_ptr<T> EnsureRenderSystem(hgl::ecs::ECSContext* ctx)
    {
        if (!ctx)
            return nullptr;

        auto system = ctx->GetSystem<T>();
        if (!system)
            system = ctx->RegisterRenderSystem<T>();

        return system;
    }

    bool InstallPrimitiveGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        auto *device = ctx->GetGPUDevice();
        const hgl::graph::CameraInfo *camera_info = nullptr;
        if (auto camera_system = ctx->GetSystem<hgl::ecs::CameraSystem>())
            camera_info = camera_system->GetCameraInfo();

        auto quad_resource_prepare_system = EnsureRenderSystem<hgl::ecs::QuadResourcePrepareSystem>(ctx);
        if (quad_resource_prepare_system)
            quad_resource_prepare_system->SetWorld(ctx);

        auto quad_mesh_prepare_system = EnsureRenderSystem<hgl::ecs::QuadMeshPrepareSystem>(ctx);
        if (quad_mesh_prepare_system)
            quad_mesh_prepare_system->SetWorld(ctx);

        // MaterialResolveSystem
        auto material_resolve_system = EnsureRenderSystem<hgl::ecs::MaterialResolveSystem>(ctx);
        if (material_resolve_system)
            material_resolve_system->SetWorld(ctx);

        // TextureMaterialBindingSystem: generic texture/sampler binding for Primitive + TextureBinding
        auto texture_binding_system = EnsureRenderSystem<hgl::ecs::TextureMaterialBindingSystem>(ctx);
        if (texture_binding_system)
            texture_binding_system->SetWorld(ctx);

        auto primitive_binding_commit_system = EnsureRenderSystem<hgl::ecs::PrimitiveBindingCommitSystem>(ctx);
        if (primitive_binding_commit_system)
            primitive_binding_commit_system->SetWorld(ctx);

        // Collect system stays: gathers PrimitiveComponents into RenderFrameCache
        auto render_collect_system = EnsureRenderSystem<hgl::ecs::RenderPrimitiveCollectSystem>(ctx);
        if (render_collect_system)
        {
            render_collect_system->SetWorld(ctx);
            render_collect_system->SetCameraInfo(camera_info);
        }

        // Buffer upload is a shared utility system, not Primitive-specific
        auto render_upload_system = EnsureRenderSystem<hgl::ecs::RenderBufferUploadSystem>(ctx);
        if (render_upload_system)
        {
            render_upload_system->SetWorld(ctx);
            render_upload_system->SetDevice(device);
        }

        // New unified pipeline group replaces Cull/Sort/Build/Finalize/Submit systems
        hgl::ecs::PrimitiveRenderPipelineGroup group;
        group.Initialize(ctx);

        return true;
    }

    bool InstallTextGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        // New unified pipeline group replaces TextCollect/Build/Sync/Submit systems
        hgl::ecs::TextRenderPipelineGroup group;
        group.Initialize(ctx);

        return true;
    }

    bool InstallBillboardGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        // New unified pipeline group replaces inline system registration
        hgl::ecs::BillboardRenderPipelineGroup group;
        group.Initialize(ctx);

        return true;
    }

    bool InstallTerrainGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        hgl::ecs::TerrainRenderPipelineGroup group;
        group.Initialize(ctx);

        return true;
    }

    bool InstallLineGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        // LineBoundsUpdateSystem is a tick system, stays outside the group
        auto line_bounds_update_system = EnsureTickSystem<hgl::ecs::LineBoundsUpdateSystem>(ctx);
        if (line_bounds_update_system)
            line_bounds_update_system->SetWorld(ctx);

        // New unified pipeline group replaces old LineCollectSystem + LineRenderSystem
        hgl::ecs::LineRenderPipelineGroup group;
        group.Initialize(ctx);

        // Stats system (thin stats logger, not part of the pipeline group)
        EnsureRenderSystem<hgl::ecs::LineStatsSystem>(ctx);

        return true;
    }

    bool InstallAssetInstanceGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        const hgl::graph::CameraInfo *camera_info = nullptr;
        if (auto camera_system = ctx->GetSystem<hgl::ecs::CameraSystem>())
            camera_info = camera_system->GetCameraInfo();

        auto collect_system = EnsureRenderSystem<hgl::ecs::AssetInstanceCollectSystem>(ctx);
        if (collect_system)
        {
            collect_system->SetWorld(ctx);
            collect_system->SetCameraInfo(camera_info);
        }

        return true;
    }

    void RegisterSystemGroupInstallers()
    {
        static bool registered = false;
        if (registered)
            return;

        auto& registry = hgl::ecs::SystemGroupRegistry::Get();
        registry.RegisterGroupInstaller("Primitive", InstallPrimitiveGroup);
        registry.RegisterGroupInstaller("AssetInstance", InstallAssetInstanceGroup);
        registry.RegisterGroupInstaller("Text", InstallTextGroup);
        registry.RegisterGroupInstaller("Billboard", InstallBillboardGroup);
        registry.RegisterGroupInstaller("Line", InstallLineGroup);
        registry.RegisterGroupInstaller("Terrain", InstallTerrainGroup);

        registered = true;
    }
}

namespace hgl::ecs
{
    static void EnsureCoreEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt)
    {
        auto *rc = ctx->GetRenderContext();

        auto input_system                       = EnsureTickSystem<ecs::InputSystem>(ctx);
        auto camera_system                      = EnsureTickSystem<ecs::CameraSystem>(ctx);
        auto environment_system                 = EnsureRenderSystem<ecs::EnvironmentSystem>(ctx);
        auto render_target_system               = EnsureRenderSystem<ecs::RenderTargetSystem>(ctx);
        auto swapchain_next_image_system        = EnsureRenderSystem<ecs::SwapchainNextImageSystem>(ctx);
        auto swapchain_submit_system            = EnsureRenderSystem<ecs::SwapchainSubmitSystem>(ctx);
        auto render_frame_business_sync_system  = EnsureRenderSystem<ecs::RenderFrameBusinessSyncSystem>(ctx);
        auto render_descriptor_binding_system   = EnsureRenderSystem<ecs::RenderDescriptorBindingSystem>(ctx);

        (void)input_system;
        (void)swapchain_next_image_system;
        (void)swapchain_submit_system;
        (void)render_frame_business_sync_system;
        (void)render_descriptor_binding_system;

        if (environment_system)
            environment_system->SetRenderContext(rc);

        if (render_target_system)
        {
            render_target_system->SetRenderContext(rc);
            if (camera_system)
                camera_system->SetViewportInfo(default_rt ? default_rt->GetViewportInfo() : nullptr);
        }
    }

    bool EnsureSystemGroupSystems(ECSContext *ctx, const std::string& group_name, graph::IRenderTarget *default_rt)
    {
        if (!ctx || group_name.empty())
            return false;

        EnsureCoreEcsSystems(ctx, default_rt);
        RegisterSystemGroupInstallers();

        if (ctx->IsSystemGroupInstalled(group_name))
            return true;

        auto& registry = SystemGroupRegistry::Get();
        const bool installed = registry.EnsureGroupSystems(group_name, ctx, default_rt);
        if (!installed)
            return false;

        ctx->MarkSystemGroupInstalled(group_name);
        ctx->GetSystemProfiler().MarkGroupEnsured(group_name);
        return true;
    }

    DefaultEcsSystems RegisterDefaultEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt)
    {
        DefaultEcsSystems systems;
        if (!ctx)
            return systems;

        EnsureCoreEcsSystems(ctx, default_rt);
        RegisterSystemGroupInstallers();
        EnsureSystemGroupSystems(ctx, "Primitive", default_rt);
        EnsureSystemGroupSystems(ctx, "Text", default_rt);
        EnsureSystemGroupSystems(ctx, "Billboard", default_rt);
        EnsureSystemGroupSystems(ctx, "Line", default_rt);

        systems.input_system                = ctx->GetSystem<ecs::InputSystem>();
        systems.camera_system               = ctx->GetSystem<ecs::CameraSystem>();
        systems.line_bounds_update_system   = ctx->GetSystem<ecs::LineBoundsUpdateSystem>();
        systems.line_collect_system         = ctx->GetSystem<ecs::LineCollectSystem>();
        systems.line_render_system          = ctx->GetSystem<ecs::LineRenderSystem>();
        systems.line_stats_system           = ctx->GetSystem<ecs::LineStatsSystem>();

        return systems;
    }
}

