#include <hgl/ecs/core/DefaultSystems.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/SystemGroup.h>
#include <hgl/ecs/systems/tick/InputSystem.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/tick/LineBoundsUpdateSystem.h>
#include <hgl/ecs/systems/render/TextCollectSystem.h>
#include <hgl/ecs/systems/render/TextBuildSystem.h>
#include <hgl/ecs/systems/render/TextResourceSyncSystem.h>
#include <hgl/ecs/systems/render/TextRenderSubmitSystem.h>
#include <hgl/ecs/systems/render/LineCollectSystem.h>
#include <hgl/ecs/systems/render/LineBufferPrepareSystem.h>
#include <hgl/ecs/systems/render/LineStatsSystem.h>
#include <hgl/ecs/systems/render/EnvironmentSystem.h>
#include <hgl/ecs/systems/render/RenderTargetSystem.h>
#include <hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include <hgl/ecs/systems/render/RenderPrimitiveCullSystem.h>
#include <hgl/ecs/systems/render/RenderPrimitiveSortSystem.h>
#include <hgl/ecs/systems/render/RenderPrimitiveBatchBuildSystem.h>
#include <hgl/ecs/systems/render/RenderPrimitiveBatchFinalizeSystem.h>
#include <hgl/ecs/systems/render/RenderPrimitiveSubmitSystem.h>
#include <hgl/ecs/systems/render/RenderBufferCommitSystem.h>
#include <hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include <hgl/ecs/systems/render/SwapchainNextImageSystem.h>
#include <hgl/ecs/systems/render/SwapchainSubmitSystem.h>
#include <hgl/ecs/systems/render/LineRenderSystem.h>
#include <hgl/ecs/systems/render/QuadResourcePrepareSystem.h>
#include <hgl/ecs/systems/render/QuadMaterialBindingSystem.h>
#include <hgl/ecs/systems/render/RenderFrameBusinessSyncSystem.h>
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

        auto render_collect_system = EnsureRenderSystem<hgl::ecs::RenderPrimitiveCollectSystem>(ctx);
        auto render_cull_system = EnsureRenderSystem<hgl::ecs::RenderPrimitiveCullSystem>(ctx);
        auto render_sort_system = EnsureRenderSystem<hgl::ecs::RenderPrimitiveSortSystem>(ctx);
        auto render_batch_build_system = EnsureRenderSystem<hgl::ecs::RenderPrimitiveBatchBuildSystem>(ctx);
        auto render_batch_finalize_system = EnsureRenderSystem<hgl::ecs::RenderPrimitiveBatchFinalizeSystem>(ctx);
        auto render_commit_system = EnsureRenderSystem<hgl::ecs::RenderBufferCommitSystem>(ctx);
        auto render_upload_system = EnsureRenderSystem<hgl::ecs::RenderBufferUploadSystem>(ctx);
        auto render_submit_system = EnsureRenderSystem<hgl::ecs::RenderPrimitiveSubmitSystem>(ctx);

        if (render_collect_system)
        {
            render_collect_system->SetWorld(ctx);
            render_collect_system->SetCameraInfo(camera_info);
        }

        if (render_commit_system)
        {
            render_commit_system->SetWorld(ctx);
            render_commit_system->SetDevice(device);
        }

        if (render_upload_system)
        {
            render_upload_system->SetWorld(ctx);
            render_upload_system->SetDevice(device);
        }

        if (render_submit_system)
            render_submit_system->SetWorld(ctx);

        (void)render_cull_system;
        (void)render_sort_system;
        (void)render_batch_build_system;
        (void)render_batch_finalize_system;
        return true;
    }

    bool InstallTextGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        auto text_collect_system = EnsureRenderSystem<hgl::ecs::TextCollectSystem>(ctx);
        auto text_build_system = EnsureRenderSystem<hgl::ecs::TextBuildSystem>(ctx);
        auto text_sync_system = EnsureRenderSystem<hgl::ecs::TextResourceSyncSystem>(ctx);
        auto text_submit_system = EnsureRenderSystem<hgl::ecs::TextRenderSubmitSystem>(ctx);

        if (text_submit_system)
            text_submit_system->SetWorld(ctx);

        (void)text_collect_system;
        (void)text_build_system;
        (void)text_sync_system;
        return true;
    }

    bool InstallBillboardGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        auto quad_resource_prepare_system = EnsureRenderSystem<hgl::ecs::QuadResourcePrepareSystem>(ctx);
        auto quad_material_binding_system = EnsureRenderSystem<hgl::ecs::QuadMaterialBindingSystem>(ctx);

        if (quad_resource_prepare_system)
            quad_resource_prepare_system->SetWorld(ctx);

        if (quad_material_binding_system)
            quad_material_binding_system->SetWorld(ctx);

        return true;
    }

    bool InstallLineGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        auto line_bounds_update_system = EnsureTickSystem<hgl::ecs::LineBoundsUpdateSystem>(ctx);
        auto line_collect_system = EnsureRenderSystem<hgl::ecs::LineCollectSystem>(ctx);
        auto line_prepare_system = EnsureRenderSystem<hgl::ecs::LineBufferPrepareSystem>(ctx);
        auto line_render_system = EnsureRenderSystem<hgl::ecs::LineRenderSystem>(ctx);
        auto line_stats_system = EnsureRenderSystem<hgl::ecs::LineStatsSystem>(ctx);

        if (line_collect_system)
            line_collect_system->SetWorld(ctx);

        if (line_bounds_update_system)
            line_bounds_update_system->SetWorld(ctx);

        if (line_stats_system)
            line_stats_system->SetWorld(ctx);

        if (line_prepare_system)
            line_prepare_system->SetWorld(ctx);

        (void)line_render_system;
        return true;
    }

    void RegisterBuiltinSystemGroupInstallers()
    {
        static bool registered = false;
        if (registered)
            return;

        auto& registry = hgl::ecs::SystemGroupRegistry::Get();
        registry.RegisterGroupInstaller("Primitive", InstallPrimitiveGroup);
        registry.RegisterGroupInstaller("Text", InstallTextGroup);
        registry.RegisterGroupInstaller("Billboard", InstallBillboardGroup);
        registry.RegisterGroupInstaller("Line", InstallLineGroup);

        registered = true;
    }
}

namespace hgl::ecs
{
    void EnsureCoreEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt)
    {
        if (!ctx)
            return;

        RegisterBuiltinSystemGroupInstallers();

        auto *rc = ctx->GetRenderContext();

        auto input_system = EnsureTickSystem<ecs::InputSystem>(ctx);
        auto camera_system = EnsureTickSystem<ecs::CameraSystem>(ctx);
        auto environment_system = EnsureRenderSystem<ecs::EnvironmentSystem>(ctx);
        auto render_target_system = EnsureRenderSystem<ecs::RenderTargetSystem>(ctx);
        auto swapchain_next_image_system = EnsureRenderSystem<ecs::SwapchainNextImageSystem>(ctx);
        auto swapchain_submit_system = EnsureRenderSystem<ecs::SwapchainSubmitSystem>(ctx);
        auto render_frame_business_sync_system = EnsureRenderSystem<ecs::RenderFrameBusinessSyncSystem>(ctx);

        (void)input_system;
        (void)swapchain_next_image_system;
        (void)swapchain_submit_system;
        (void)render_frame_business_sync_system;

        if (environment_system)
            environment_system->SetRenderContext(rc);

        if (camera_system)
        {
            camera_system->SetRenderContext(rc);
            camera_system->SetViewportInfo(default_rt ? default_rt->GetViewportInfo() : nullptr);
        }

        if (render_target_system)
        {
            render_target_system->SetRenderContext(rc);
            render_target_system->SetRenderTarget(default_rt ? default_rt : ctx->GetRenderTarget());
        }
    }

    bool EnsureSystemGroupSystems(ECSContext *ctx, const std::string& group_name, graph::IRenderTarget *default_rt)
    {
        if (!ctx || group_name.empty())
            return false;

        EnsureCoreEcsSystems(ctx, default_rt);
        ctx->GetSystemProfiler().MarkGroupEnsured(group_name);

        auto& registry = SystemGroupRegistry::Get();
        return registry.EnsureGroupSystems(group_name, ctx, default_rt);
    }

    DefaultEcsSystems RegisterDefaultEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt)
    {
        DefaultEcsSystems systems;
        if (!ctx)
            return systems;

        EnsureCoreEcsSystems(ctx, default_rt);
        EnsureSystemGroupSystems(ctx, "Primitive", default_rt);
        EnsureSystemGroupSystems(ctx, "Text", default_rt);
        EnsureSystemGroupSystems(ctx, "Billboard", default_rt);
        EnsureSystemGroupSystems(ctx, "Line", default_rt);

        systems.input_system = ctx->GetSystem<ecs::InputSystem>();
        systems.camera_system = ctx->GetSystem<ecs::CameraSystem>();
        systems.line_bounds_update_system = ctx->GetSystem<ecs::LineBoundsUpdateSystem>();
        systems.line_collect_system = ctx->GetSystem<ecs::LineCollectSystem>();
        systems.line_render_system = ctx->GetSystem<ecs::LineRenderSystem>();
        systems.line_stats_system = ctx->GetSystem<ecs::LineStatsSystem>();

        return systems;
    }
}
