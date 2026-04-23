#pragma once
#include <hgl/ecs/support/RenderPipelineGroup.h>

namespace hgl::ecs
{
    /**
     * Sprite2DRenderPipelineGroup
     *
     * Wraps the two Sprite2D setup systems:
     *   - Sprite2DResourcePrepareSystem  (RenderResourceSetup phase)
     *   - Sprite2DMaterialBindingSystem  (RenderMaterialBind  phase)
     *
     * Register with:
     *   EnsureSystemGroupSystems(ctx, "Sprite2D", render_target);
     *
     * This group is opt-in — it is NOT auto-installed by RegisterDefaultEcsSystems().
     * Sprite2D geometry is collected by the shared Primitive pipeline
     * (RenderPrimitiveCollectSystem) — no separate draw stage is needed here.
     */
    class Sprite2DRenderPipelineGroup : public RenderPipelineGroup
    {
    public:
        Sprite2DRenderPipelineGroup();
        ~Sprite2DRenderPipelineGroup() override = default;

        bool Initialize(ECSContext* context) override;
        void Shutdown(ECSContext* context)   override;

    protected:
        std::unique_ptr<RenderPipelineBase> CreatePipeline() override;
        void RegisterSystems() override;
    };

}  // namespace hgl::ecs
