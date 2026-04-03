#pragma once

#include <hgl/ecs/support/RenderPipelineGroup.h>

namespace hgl::ecs
{
    /**
     * PrimitiveRenderPipelineGroup - Container for Primitive rendering pipeline and systems
     * 
     * Encapsulates the complete primitive rendering stack:
     *   - PrimitiveRenderPipeline (wraps PrimitiveBatchPipeline)
     *   - PrimitiveCullSystem  (RenderBatch phase, calls pipeline->RunCull)
     *   - PrimitiveSortSystem  (RenderBatch phase, calls pipeline->RunSort)
     *   - PrimitiveBuildSystem (RenderBatch phase, calls pipeline->RunBuild)
     *   - PrimitiveRenderSystem (RenderDrawSubmit phase, calls pipeline->Render)
     * 
     * Usage:
     *   auto group = std::make_unique<PrimitiveRenderPipelineGroup>();
     *   group->Initialize(context);
     *   // -> GraphicsPipeline registered as "Primitive" in Context's pipeline map
     *   // -> All systems registered to Context and assigned correct ExecutionPhase
     */
    class PrimitiveRenderPipelineGroup : public RenderPipelineGroup
    {
    public:
        PrimitiveRenderPipelineGroup();
        ~PrimitiveRenderPipelineGroup() override = default;

        bool Initialize(ECSContext* context) override;
        void Shutdown(ECSContext* context) override;

    protected:
        std::unique_ptr<RenderPipelineBase> CreatePipeline() override;
        void RegisterSystems() override;
    };

}  // namespace hgl::ecs
