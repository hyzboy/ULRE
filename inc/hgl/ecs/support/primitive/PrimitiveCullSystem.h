#pragma once

#include <hgl/ecs/support/RenderPipelineSystem.h>

namespace hgl::ecs
{
    /**
     * PrimitiveCullSystem - RenderCull phase for primitives
     * 
     * Performs frustum culling and visibility tests on collected primitive items.
     * Derived from CullSystem to provide unified System interface.
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
