#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/support/render/WorldRenderRunner.h>
#include <hgl/graph/core/GraphicsContext.h>

namespace hgl::ecs
{
    void ECSContext::OnSwapchainTargetChanged(graph::IRenderTarget *target)
    {
        render_runner.OnSwapchainTargetChanged(target);
    }

    void ECSContext::OnRenderTargetResized(graph::IRenderTarget *target, uint32 width, uint32 height)
    {
        render_runner.OnRenderTargetResized(target, width, height);
    }

    bool ECSContext::EnsureRenderCoreInitialized()
    {
        return render_runner.EnsureRenderCoreInitialized();
    }

    bool ECSContext::HasPendingRenderableResources() const
    {
        return render_runner.HasPendingRenderableResources();
    }

    void ECSContext::ExecuteScenePrePassWorkflow(float deltaTime)
    {
        render_runner.ExecuteScenePrePassWorkflow(deltaTime);
    }

    bool ECSContext::BeginManagedRenderFrame(float deltaTime, const bool need_swapchain_acquire, const graph::RenderPassOptions *options)
    {
        return render_runner.BeginManagedRenderFrame(deltaTime, need_swapchain_acquire, options);
    }

    bool ECSContext::EndManagedRenderFrame(float deltaTime)
    {
        return render_runner.EndManagedRenderFrame(deltaTime);
    }

    void ECSContext::RecordPreparedRenderPhaseRange(ExecutionPhase minPhase,
                                                    ExecutionPhase maxPhase,
                                                    float deltaTime,
                                                    bool submit_transforms,
                                                    const char *log_prefix)
    {
        render_runner.RecordPreparedRenderPhaseRange(minPhase, maxPhase, deltaTime, submit_transforms, log_prefix);
    }

    void ECSContext::RenderDrawOnly(graph::RenderCmdBuffer* cmd, float deltaTime)
    {
        render_runner.RenderDrawOnly(cmd, deltaTime);
    }

    bool ECSContext::RenderTo(const RenderPassRequest &req)
    {
        return render_runner.RenderTo(req);
    }

    bool ECSContext::RenderTo(graph::IRenderTarget *rt, const hgl::Color4f &clear, float deltaTime, CullMode cull_mode)
    {
        return render_runner.RenderTo(rt, clear, deltaTime, cull_mode);
    }

    void ECSContext::OnResize(const VkExtent2D &extent)
    {
        render_runner.OnResize(extent);
    }

    void ECSContext::Render(float deltaTime, const std::function<void(float)> &pre_render)
    {
        auto &topology = render_runner.GetMainFrameTopology();
        auto &cache = render_runner.GetAdaptiveGraphCache();
        topology.BeginFrame(GetGraphicsContext() ? GetGraphicsContext()->GetGlobalSSBOBufferRegistry() : nullptr);

        if (cache.IsDirty())
        {
            SceneStats stats = GatherSceneStats(this);
            const uint64_t current_hash = stats.GetHash();

            if (cache.NeedsRebuild(current_hash))
                cache.Store(current_hash, CreateAdaptiveRenderGraph(this, stats));

            cache.MarkClean();
        }

        Render(deltaTime, cache.Graph(), pre_render);
    }

    void ECSContext::RenderPreBeginFrame(float deltaTime)
    {
        render_runner.RenderPreBeginFrame(deltaTime);
    }

    void ECSContext::RenderSwapchainNextImage(float deltaTime)
    {
        render_runner.RenderSwapchainNextImage(deltaTime);
    }

    bool ECSContext::AcquireSwapchainImage(float deltaTime)
    {
        return render_runner.AcquireSwapchainImage(deltaTime);
    }

    void ECSContext::SyncRenderTargetViewportToExtent()
    {
        render_runner.SyncRenderTargetViewportToExtent();
    }

    void ECSContext::RenderBufferCommit(float deltaTime)
    {
        render_runner.RenderBufferCommit(deltaTime);
    }

    void ECSContext::RenderBufferUpload(float deltaTime)
    {
        render_runner.RenderBufferUpload(deltaTime);
    }

    void ECSContext::RenderFrameSync(float deltaTime)
    {
        render_runner.RenderFrameSync(deltaTime);
    }

    void ECSContext::PrepareRenderPassSetup(uint32_t frameIndex, float deltaTime)
    {
        render_runner.PrepareRenderPassSetup(frameIndex, deltaTime);
    }

    void ECSContext::RenderSubmit(float deltaTime)
    {
        render_runner.RenderSubmit(deltaTime);
    }

    bool ECSContext::SubmitFrameToRenderTarget(float deltaTime)
    {
        return render_runner.SubmitFrameToRenderTarget(deltaTime);
    }

    bool ECSContext::RecreateSwapchainIfNeeded()
    {
        return render_runner.RecreateSwapchainIfNeeded();
    }

    void ECSContext::RunRenderPhaseUpdates(ExecutionPhase phase, float deltaTime)
    {
        render_runner.RunRenderPhaseUpdates(phase, deltaTime);
    }

    void ECSContext::RunRenderPhaseUpdates(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime)
    {
        render_runner.RunRenderPhaseUpdates(minPhase, maxPhase, deltaTime);
    }

    void ECSContext::RunSystemUpdate(System *system, float deltaTime)
    {
        render_runner.RunSystemUpdate(system, deltaTime);
    }

    void ECSContext::RunRenderSystemsInRange(ExecutionPhase minPhase, ExecutionPhase maxPhase, float deltaTime)
    {
        render_runner.RunRenderSystemsInRange(minPhase, maxPhase, deltaTime);
    }
} // namespace hgl::ecs