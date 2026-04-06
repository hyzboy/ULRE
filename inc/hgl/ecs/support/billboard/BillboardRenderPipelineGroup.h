#pragma once
#include <hgl/ecs/support/RenderPipelineGroup.h>

namespace hgl::ecs
{
    /**
     * BillboardRenderPipelineGroup
     *
     * Wraps the two Quad/Billboard setup systems:
     *   - QuadResourcePrepareSystem  (RenderResourceSetup phase)
     *   - QuadMaterialBindingSystem  (RenderMaterialBind  phase)
     *
     * Note: Billboard geometry is rendered by the Primitive pipeline via
     * RenderPrimitiveCollectSystem — there is no separate render-draw stage here.
     */
    class BillboardRenderPipelineGroup : public RenderPipelineGroup
    {
    public:
        BillboardRenderPipelineGroup();
        ~BillboardRenderPipelineGroup() override = default;

        bool Initialize(ECSContext* context) override;
        void Shutdown(ECSContext* context)   override;

    protected:
        std::unique_ptr<RenderPipelineBase> CreatePipeline() override;
        void RegisterSystems() override;
    };

}  // namespace hgl::ecs
