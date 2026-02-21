#include <hgl/ecs/core/DefaultSystems.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/systems/tick/InputSystem.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/ecs/systems/render/TextCollectSystem.h>
#include <hgl/ecs/systems/render/TextBuildSystem.h>
#include <hgl/ecs/systems/render/TextResourceSyncSystem.h>
#include <hgl/ecs/systems/render/TextRenderSubmitSystem.h>
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
#include <hgl/graph/render/RenderContext.h>
#include <hgl/vk/VKRenderTarget.h>

namespace hgl::ecs
{
    DefaultEcsSystems RegisterDefaultEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt)
    {
        DefaultEcsSystems systems;
        if (!ctx)
            return systems;

        // Phase 1 boundary: RenderFrameSystem is not registered here.
        // Frame lifecycle is driven by ECSContext::Render(float) + RenderSystemCore.
        auto *rc = ctx->GetRenderContext();
        auto *device = ctx->GetGPUDevice();

        auto text_collect_system = ctx->RegisterRenderSystem<ecs::TextCollectSystem>();
        auto text_build_system = ctx->RegisterRenderSystem<ecs::TextBuildSystem>();
        auto text_sync_system = ctx->RegisterRenderSystem<ecs::TextResourceSyncSystem>();
        auto environment_system = ctx->RegisterRenderSystem<ecs::EnvironmentSystem>();
        auto camera_system = ctx->RegisterTickSystem<ecs::CameraSystem>();
        auto render_target_system = ctx->RegisterRenderSystem<ecs::RenderTargetSystem>();
        auto render_collect_system = ctx->RegisterRenderSystem<ecs::RenderPrimitiveCollectSystem>();
        auto render_cull_system = ctx->RegisterRenderSystem<ecs::RenderPrimitiveCullSystem>();
        auto render_sort_system = ctx->RegisterRenderSystem<ecs::RenderPrimitiveSortSystem>();
        auto render_batch_build_system = ctx->RegisterRenderSystem<ecs::RenderPrimitiveBatchBuildSystem>();
        auto render_batch_finalize_system = ctx->RegisterRenderSystem<ecs::RenderPrimitiveBatchFinalizeSystem>();
        auto render_commit_system = ctx->RegisterRenderSystem<ecs::RenderBufferCommitSystem>();
        auto render_upload_system = ctx->RegisterRenderSystem<ecs::RenderBufferUploadSystem>();
        auto render_submit_system = ctx->RegisterRenderSystem<ecs::RenderPrimitiveSubmitSystem>();
        auto swapchain_next_image_system = ctx->RegisterRenderSystem<ecs::SwapchainNextImageSystem>();
        auto swapchain_submit_system = ctx->RegisterRenderSystem<ecs::SwapchainSubmitSystem>();
        auto text_submit_system = ctx->RegisterRenderSystem<ecs::TextRenderSubmitSystem>();
        auto line_render_system = ctx->RegisterRenderSystem<ecs::LineRenderSystem>();
        auto quad_resource_prepare_system = ctx->RegisterRenderSystem<ecs::QuadResourcePrepareSystem>();
        auto quad_material_binding_system = ctx->RegisterRenderSystem<ecs::QuadMaterialBindingSystem>();

        (void)swapchain_next_image_system;
        (void)swapchain_submit_system;

        (void)text_collect_system;
        (void)text_build_system;
        (void)text_sync_system;

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
            render_target_system->SetRenderTarget(default_rt);
        }

        const graph::CameraInfo *camera_info = camera_system ? camera_system->GetCameraInfo() : nullptr;

        if (render_collect_system)
        {
            render_collect_system->SetWorld(ctx);
            render_collect_system->SetCameraInfo(camera_info);
        }

        (void)render_cull_system;
        (void)render_sort_system;
        (void)render_batch_build_system;
        (void)render_batch_finalize_system;

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

        if (text_submit_system)
            text_submit_system->SetWorld(ctx);

        if (quad_resource_prepare_system)
            quad_resource_prepare_system->SetWorld(ctx);

        if (quad_material_binding_system)
            quad_material_binding_system->SetWorld(ctx);

        // CN: LineRenderSystem 会在 Render 时延迟初始化，自动获取 context 中的信息
        // EN: LineRenderSystem will lazy-init on first Render, automatically get context info

        systems.input_system = ctx->RegisterTickSystem<ecs::InputSystem>();
        systems.camera_system = camera_system;
        systems.line_render_system = line_render_system;

        return systems;
    }
}
