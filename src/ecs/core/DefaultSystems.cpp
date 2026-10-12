#include <hgl/ecs/core/DefaultSystems.h>
#include<hgl/common/RenderOptions.h>   // 编译期开关台帐（§2 校验面）
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
#include <hgl/ecs/support/TextRenderPipeline.h>
#include <hgl/ecs/support/text/TextCollectSystem.h>
#include <hgl/ecs/support/text/TextBuildSystem.h>
#include <hgl/ecs/support/text/TextSyncSystem.h>
#include <hgl/ecs/support/text/TextRenderSystem.h>
#include <hgl/ecs/systems/render/LineStatsSystem.h>
#include <hgl/ecs/systems/render/EnvironmentSystem.h>
#include <hgl/ecs/systems/render/LightSystem.h>
#include <hgl/ecs/systems/render/MainLightShadowSystem.h>
#include <hgl/ecs/systems/render/ColorPaletteSystem.h>
#include <hgl/ecs/systems/render/RenderTargetSystem.h>
#include <hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/support/primitive/PrimitiveCullSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveSortSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveBuildSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveOverlayRenderSystem.h>
#include <hgl/ecs/support/GPUCullSystem.h>
#include <hgl/ecs/support/CPUCullReferenceSystem.h>
#include <hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include <hgl/ecs/systems/render/SwapchainNextImageSystem.h>
#include <hgl/ecs/systems/render/SwapchainSubmitSystem.h>
#include <hgl/ecs/systems/render/ViewUBOCommitSystem.h>
#include <hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/vk/VKRenderTarget.h>

namespace
{
    /// 幂等注册：已有就取回，没有才注册。tick / render 由系统自身相位决定，故不再分两个包装。
    template<typename T>
    std::shared_ptr<T> EnsureSystem(hgl::ecs::ECSContext* ctx)
    {
        if (!ctx)
            return nullptr;

        auto system = ctx->GetSystem<T>();
        if (!system)
            system = ctx->RegisterSystem<T>();

        return system;
    }

    bool InstallPrimitiveGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        auto *device = ctx->GetGPUDevice();

        // Collect system stays: gathers primitive entities (GeometryData) into RenderFrameCache
        auto render_collect_system = EnsureSystem<hgl::ecs::RenderPrimitiveCollectSystem>(ctx);
        if (render_collect_system)
        {
            render_collect_system->SetWorld(ctx);
        }

        // Buffer upload is a shared utility system, not Primitive-specific
        auto render_upload_system = EnsureSystem<hgl::ecs::RenderBufferUploadSystem>(ctx);
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
        ctx->RegisterSystem<hgl::ecs::PrimitiveCullSystem>();
        ctx->RegisterSystem<hgl::ecs::PrimitiveSortSystem>();
        ctx->RegisterSystem<hgl::ecs::PrimitiveBuildSystem>();
        ctx->RegisterSystem<hgl::ecs::PrimitiveRenderSystem>();
        ctx->RegisterSystem<hgl::ecs::PrimitiveOverlayRenderSystem>();

        // GPU cull system for entity-based frustum culling (T3d-1b)
        auto gpu_cull_system = EnsureSystem<hgl::ecs::GPUCullSystem>(ctx);
        if (gpu_cull_system)
        {
            gpu_cull_system->Initialize(ctx);
            // A6-d T2：接管已强制（opt-in 面整体删除）——dispatch 与 gather 接管恒开；
            // 实际是否接管某批仍由 PrimitiveBatchPipeline 的逐批 guard 判定。
        }

#if ULRE_ENABLE_CPU_CULL_PARITY_TEST
        // CPU cull reference system for parity testing (T3d-2) - 仅在显式开启对拍测试时注册，避免生产管线每 60 帧 GPU stall
        auto cpu_cull_system = EnsureSystem<hgl::ecs::CPUCullReferenceSystem>(ctx);
        if (cpu_cull_system)
        {
            cpu_cull_system->Initialize(ctx);
        }
#endif

        // T3f-2：GPU 剔除结果接管绘制改由 `PrimitiveBatchPipeline::TryApplyGPUCullOverride`
        // （FinalizeBatches 开头、批建成之后逐帧 guard）+ `GPUCullSystem::ApplyGPUCullOverrideHook`
        // 承担——原 `GPUCullResultHookSystem`（RenderFrameSync 阶段"挂第一个批、清其余批"）
        // 的接线方式与批生命周期不匹配（跨帧批对象、items 清零丢绘制），随本批删除。

        return true;
    }

    bool InstallTextGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        // Register the Text pipeline (direct RenderPipelineBase derived) and its
        // thin proxy systems directly
        auto text_pipeline = std::make_unique<hgl::ecs::TextRenderPipeline>();
        text_pipeline->SetWorld(ctx);
        text_pipeline->SetRenderContext(ctx->GetRenderContext());
        ctx->RegisterRenderPipeline("Text", std::move(text_pipeline));
        ctx->RegisterSystem<hgl::ecs::TextCollectSystem>();
        ctx->RegisterSystem<hgl::ecs::TextBuildSystem>();
        ctx->RegisterSystem<hgl::ecs::TextSyncSystem>();
        ctx->RegisterSystem<hgl::ecs::TextRenderSystem>();

        return true;
    }


    bool InstallLineGroup(hgl::ecs::ECSContext* ctx, hgl::graph::IRenderTarget* /*default_rt*/)
    {
        if (!ctx)
            return false;

        // LineBoundsUpdateSystem is a tick system, stays outside the group
        auto line_bounds_update_system = EnsureSystem<hgl::ecs::LineBoundsUpdateSystem>(ctx);
        if (line_bounds_update_system)
            line_bounds_update_system->SetWorld(ctx);

        // Register the Line pipeline and its thin proxy systems directly
        auto line_pipeline = std::make_unique<hgl::ecs::LineRenderPipeline>(ctx);
        ctx->RegisterRenderPipeline("Line", std::move(line_pipeline));
        ctx->RegisterSystem<hgl::ecs::LineCollectSystem>();
        ctx->RegisterSystem<hgl::ecs::LineBuildSystem>();
        ctx->RegisterSystem<hgl::ecs::LineRenderSystem>();

#if ULRE_ECS_DEBUG_API
        // Stats system (thin stats logger, 按需注册；开关见 RenderOptions.h §2)
        EnsureSystem<hgl::ecs::LineStatsSystem>(ctx);
#endif

        return true;
    }

    void RegisterBuiltinSystemGroupInstallers()
    {
        static bool registered = false;
        if (registered)
            return;

        auto& registry = hgl::ecs::SystemGroupRegistry::Get();
        // A2：组名统一取自 kSystemGroupNames（唯一一份字符串），不再就地写字面量
        registry.RegisterGroupInstaller(hgl::ecs::kSystemGroupNames[static_cast<uint32_t>(hgl::ecs::SystemGroupID::Primitive)], InstallPrimitiveGroup);
        registry.RegisterGroupInstaller(hgl::ecs::kSystemGroupNames[static_cast<uint32_t>(hgl::ecs::SystemGroupID::Text)],      InstallTextGroup);
        registry.RegisterGroupInstaller(hgl::ecs::kSystemGroupNames[static_cast<uint32_t>(hgl::ecs::SystemGroupID::Line)],      InstallLineGroup);

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

        EnsureSystem<ecs::InputSystem>(ctx);
        auto camera_system = EnsureSystem<ecs::CameraSystem>(ctx);
        EnsureSystem<ecs::LightSystem>(ctx);
        auto environment_system = EnsureSystem<ecs::EnvironmentSystem>(ctx);
        EnsureSystem<ecs::MainLightShadowSystem>(ctx);
        auto color_palette_system = EnsureSystem<ecs::ColorPaletteSystem>(ctx);
        auto render_target_system = EnsureSystem<ecs::RenderTargetSystem>(ctx);
        EnsureSystem<ecs::SwapchainNextImageSystem>(ctx);
        EnsureSystem<ecs::SwapchainSubmitSystem>(ctx);
        EnsureSystem<ecs::RenderSceneUBOSystem>(ctx);
        EnsureSystem<ecs::ViewUBOCommitSystem>(ctx);

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

    bool EnsureSystemGroupSystems(ECSContext *ctx, SystemGroupID group, graph::IRenderTarget *default_rt)
    {
        if (!ctx || group == SystemGroupID::None)
            return false;

        EnsureCoreEcsSystems(ctx, default_rt);

        if (ctx->IsSystemGroupInstalled(group))
            return true;

        auto& registry = SystemGroupRegistry::Get();
        const bool installed = registry.EnsureGroupSystems(
            kSystemGroupNames[static_cast<uint32_t>(group)], ctx, default_rt);
        if (!installed)
            return false;

        ctx->MarkSystemGroupInstalled(group);
        return true;
    }

    DefaultEcsSystems RegisterDefaultEcsSystems(ECSContext *ctx, graph::IRenderTarget *default_rt)
    {
        DefaultEcsSystems systems;
        if (!ctx)
            return systems;

        EnsureCoreEcsSystems(ctx, default_rt);
        EnsureSystemGroupSystems(ctx, SystemGroupID::Primitive, default_rt);
        EnsureSystemGroupSystems(ctx, SystemGroupID::Text, default_rt);
        EnsureSystemGroupSystems(ctx, SystemGroupID::Line, default_rt);

        // Prime group metadata once so graph construction never rescans
        hgl::ecs::EnsureSystemGroupsRegistered(ctx);

        systems.input_system = ctx->GetSystem<ecs::InputSystem>();

        return systems;
    }
}
