#pragma once

#include <hgl/ecs/support/pipeline/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * SimpleLineCollectSystem - RenderCollect phase for SimpleLine elements
     *
     * Calls pipeline->RunCollect() which traverses SimpleLinesComponents,
     * applies frustum culling, and stores visible components.
     * 与 LineCollectSystem 平行：仅管线名不同（"SimpleLine"）。
     */
    class SimpleLineCollectSystem : public CollectSystem
    {
    public:
        explicit SimpleLineCollectSystem(const AnsiString& name = "SimpleLineCollectSystem");
        ~SimpleLineCollectSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnCollect(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
