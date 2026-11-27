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

    void ECSContext::RunSystemUpdate(System *system, float deltaTime)
    {
        render_runner.RunSystemUpdate(system, deltaTime);
    }

} // namespace hgl::ecs