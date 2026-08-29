#include <hgl/ecs/support/text/TextCollectSystem.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>

namespace hgl::ecs
{
    TextCollectSystem::TextCollectSystem(const std::string& name)
        : CollectSystem(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderCollect);
        SetRenderElementType("Text");
        AddDependency<RenderPrimitiveCollectSystem>();
    }

    RenderPipelineBase* TextCollectSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Text");
    }

    void TextCollectSystem::OnCollect(RenderPipelineBase* pipeline)
    {
        pipeline->RunCollect();
    }

}  // namespace hgl::ecs
