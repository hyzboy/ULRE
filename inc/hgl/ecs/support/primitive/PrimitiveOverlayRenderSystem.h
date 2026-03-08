#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * PrimitiveOverlayRenderSystem - RenderDebug phase submit for overlay-like primitive batches.
     *
     * This system renders only batches whose pipeline depth state is overlay-style
     * (depthCompare=ALWAYS and depthWrite=false), e.g. gizmo overlays.
     */
    class PrimitiveOverlayRenderSystem : public RenderPipelineDrawSystem
    {
    public:
        explicit PrimitiveOverlayRenderSystem(const std::string& name = "PrimitiveOverlayRenderSystem");
        ~PrimitiveOverlayRenderSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd) override;
    };

}  // namespace hgl::ecs
