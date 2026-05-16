#include <hgl/ecs/support/billboard/BillboardRenderPipelineGroup.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    BillboardRenderPipelineGroup::BillboardRenderPipelineGroup()
        : RenderPipelineGroup("Billboard")
    {
    }

    bool BillboardRenderPipelineGroup::Initialize(ECSContext* context)
    {
        return context != nullptr;
    }

    void BillboardRenderPipelineGroup::Shutdown(ECSContext* /*context*/)
    {
        systems_.clear();
        pipeline_.reset();
    }

    std::unique_ptr<RenderPipelineBase> BillboardRenderPipelineGroup::CreatePipeline()
    {
        // No dedicated pipeline — billboard geometry goes through the Primitive pipeline
        return nullptr;
    }

    void BillboardRenderPipelineGroup::RegisterSystems()
    {
        // Systems registered directly to Context in Initialize()
    }

}  // namespace hgl::ecs
