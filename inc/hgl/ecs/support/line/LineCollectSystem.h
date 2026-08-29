#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * LineCollectSystem - RenderCollect phase for Line elements
     *
     * Calls pipeline->RunCollect() which traverses LinesComponents,
     * applies frustum culling, and stores visible components.
     * Replaces the old LineCollectSystem (systems/render/).
     */
    class LineCollectSystem : public CollectSystem
    {
    public:
        explicit LineCollectSystem(const std::string& name = "LineCollectSystem");
        ~LineCollectSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnCollect(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
