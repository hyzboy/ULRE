#include <hgl/ecs/support/text/TextRenderSystem.h>
#include <hgl/ecs/support/text/TextSyncSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include <hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    TextRenderSystem::TextRenderSystem(const std::string& name)
        : RenderPipelineDrawSystem(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderDrawSubmit);
        SetRenderElementType("Text");
        AddDependency<TextSyncSystem>();
        AddDependency<PrimitiveRenderSystem>();
        AddDependency<RenderBufferUploadSystem>();
    }

    RenderPipelineBase* TextRenderSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Text");
    }

    void TextRenderSystem::OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd)
    {
        pipeline->Render(cmd);
    }

}  // namespace hgl::ecs
