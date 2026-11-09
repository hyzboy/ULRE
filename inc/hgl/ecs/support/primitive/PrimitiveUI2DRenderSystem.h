#pragma once

#include <hgl/ecs/support/pipeline/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * PrimitiveUI2DRenderSystem - RenderUI2D phase submit for 2D UI primitive batches.
     *
     * Renders only batches whose pipeline config carries render_layer == UI2D
     * (sprite-bridged 2D quads), drawn after gizmo (RenderDebug) and before the
     * final PROF overlay (RenderOverlay).
     */
    class PrimitiveUI2DRenderSystem : public RenderPipelineDrawSystem
    {
    public:
        explicit PrimitiveUI2DRenderSystem(const std::string& name = "PrimitiveUI2DRenderSystem");
        ~PrimitiveUI2DRenderSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd) override;
    };

}  // namespace hgl::ecs
