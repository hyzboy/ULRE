#include <hgl/ecs/core/DefaultSystems.h>
#include <memory>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/SystemGroup.h>
#include <hgl/ecs/systems/tick/InputSystem.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/tick/LineBoundsUpdateSystem.h>
#include <hgl/ecs/support/line/LineRenderPipeline.h>
#include <hgl/ecs/support/line/LineCollectSystem.h>
#include <hgl/ecs/support/line/LineBuildSystem.h>
#include <hgl/ecs/support/line/LineRenderSystem.h>
#include <hgl/ecs/support/text/TextRenderPipelineAdapter.h>
#include <hgl/ecs/support/text/TextCollectSystem.h>
#include <hgl/ecs/support/text/TextBuildSystem.h>
#include <hgl/ecs/support/text/TextSyncSystem.h>
#include <hgl/ecs/support/text/TextRenderSystem.h>
#include <hgl/ecs/systems/render/LineStatsSystem.h>
#include <hgl/ecs/systems/render/EnvironmentSystem.h>
#include <hgl/ecs/systems/render/ColorPaletteSystem.h>
#include <hgl/ecs/systems/render/RenderTargetSystem.h>
#include <hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/support/primitive/PrimitiveCullSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveSortSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveBuildSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveOverlayRenderSystem.h>
#include <hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include <hgl/ecs/systems/render/SwapchainNextImageSystem.h>
#include <hgl/ecs/systems/render/SwapchainSubmitSystem.h>
#include <hgl/ecs/systems/render/RenderFrameBusinessSyncSystem.h>
#include <hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
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

        // Register the Primitive pipeline and its thin proxy systems directly
        // (the RenderPipelineGroup container abstraction was removed — it had
        // devolved into a one-shot installer; Context stays element-agnostic
        // via this installer mechanism instead)
        auto pipeline = std::make_unique<hgl::ecs::PrimitiveRenderPipeline>(ctx);
        ctx->RegisterRenderPipeline("Primitive", std::move(pipeline));
        ctx->RegisterRenderSystem<hgl::ecs::PrimitiveCullSystem>();
        ctx->RegisterRenderSystem<hgl::ecs::PrimitiveSortSystem>();
        ctx->RegisterRenderSystem<hgl::ecs::PrimitiveBuildSystem>();
        ctx->RegisterRenderSystem<hgl::ecs::PrimitiveRenderSystem>();
        ctx->RegisterRenderSystem<hgl::ecs::PrimitiveOverlayRenderSystem>();

        return true;
    }

    bool InstallTextGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        // Register the Text pipeline adapter and its thin proxy systems directly
        auto adapter = std::make_unique<hgl::ecs::TextRenderPipelineAdapter>(ctx);
        ctx->RegisterRenderPipeline("Text", std::move(adapter));
        ctx->RegisterRenderSystem<hgl::ecs::TextCollectSystem>();
        ctx->RegisterRenderSystem<hgl::ecs::TextBuildSystem>();
        ctx->RegisterRenderSystem<hgl::ecs::TextSyncSystem>();
        ctx->RegisterRenderSystem<hgl::ecs::TextRenderSystem>();

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

        // Register the Line pipeline and its thin proxy systems directly
        auto line_pipeline = std::make_unique<hgl::ecs::LineRenderPipeline>(ctx);
        ctx->RegisterRenderPipeline("Line", std::move(line_pipeline));
        ctx->RegisterRenderSystem<hgl::ecs::LineCollectSystem>();
        ctx->RegisterRenderSystem<hgl::ecs::LineBuildSystem>();
        ctx->RegisterRenderSystem<hgl::ecs::LineRenderSystem>();

        // Stats system (thin stats logger, not part of the pipeline)
        EnsureRenderSystem<hgl::ecs::LineStatsSystem>(ctx);

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
        auto color_palette_system = EnsureRenderSystem<ecs::ColorPaletteSystem>(ctx);
        auto render_target_system = EnsureRenderSystem<ecs::RenderTargetSystem>(ctx);
        auto swapchain_next_image_system = EnsureRenderSystem<ecs::SwapchainNextImageSystem>(ctx);
        auto swapchain_submit_system = EnsureRenderSystem<ecs::SwapchainSubmitSystem>(ctx);
        auto render_frame_business_sync_system = EnsureRenderSystem<ecs::RenderFrameBusinessSyncSystem>(ctx);
        auto render_descriptor_binding_system = EnsureRenderSystem<ecs::RenderDescriptorBindingSystem>(ctx);

        (void)input_system;
        (void)swapchain_next_image_system;
        (void)swapchain_submit_system;
        (void)render_frame_business_sync_system;
        (void)render_descriptor_binding_system;

        if (environment_system)
            environment_system->SetRenderContext(rc);

        if (color_palette_system)
            color_palette_system->SetRenderContext(rc);

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
        EnsureSystemGroupSystems(ctx, "Primitive", default_rt);
        EnsureSystemGroupSystems(ctx, "Text", default_rt);
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
