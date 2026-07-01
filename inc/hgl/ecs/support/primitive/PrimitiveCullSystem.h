#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * PrimitiveCullSystem - RenderCollect stage culling for primitives
     *
     * Performs frustum/visibility culling on collected primitive items.
     * This system currently runs in ExecutionPhase::RenderCollect.
     */
    class PrimitiveCullSystem : public CullSystem
    {
    public:
        explicit PrimitiveCullSystem(const std::string& name = "PrimitiveCullSystem");
        ~PrimitiveCullSystem() override = default;

        RenderPipelineBase* GetPipeline(ECSContext* context) override;

    private:
        void OnCull(RenderPipelineBase* pipeline) override;
    };

}  // namespace hgl::ecs
