#include <hgl/ecs/support/text/TextBuildSystem.h>
#include <hgl/ecs/support/text/TextCollectSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TextBuildSystem::TextBuildSystem(const std::string& name)
        : BuildSystem(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        SetRenderElementType("Text");
        AddDependency<TextCollectSystem>();
    }

    RenderPipelineBase* TextBuildSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Text");
    }

    void TextBuildSystem::OnBuild(RenderPipelineBase* pipeline)
    {
        pipeline->RunBuild();
    }

}  // namespace hgl::ecs
