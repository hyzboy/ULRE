#include <hgl/ecs/support/text/TextSyncSystem.h>
#include <hgl/ecs/support/text/TextBuildSystem.h>
#include <hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TextSyncSystem::TextSyncSystem(const std::string& name)
        : SyncSystem(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderFrameSync);
        SetRenderElementType("Text");
        AddDependency<TextBuildSystem>();
        AddDependency<RenderBufferUploadSystem>();
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
