#include <hgl/ecs/support/text/TextSyncSystem.h>
#include <hgl/ecs/support/text/TextBuildSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TextSyncSystem::TextSyncSystem(const std::string& name)
        : SyncSystem(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderBatch);
        SetRenderElementType("Text");
        AddDependency<TextBuildSystem>();
    }

    RenderPipelineBase* TextSyncSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Text");
    }

    void TextSyncSystem::OnSync(RenderPipelineBase* pipeline)
    {
        pipeline->RunSync();
    }

}  // namespace hgl::ecs
