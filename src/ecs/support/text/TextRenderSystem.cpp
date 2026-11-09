#include <hgl/ecs/support/text/TextRenderSystem.h>
#include <hgl/ecs/support/text/TextRenderPipeline.h>
#include <hgl/ecs/support/text/TextSyncSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include <hgl/ecs/systems/render/buffer/RenderBufferUploadSystem.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/mtl/RenderLayer.h>

namespace hgl::ecs
{
    TextRenderSystem::TextRenderSystem(const std::string& name)
        : RenderPipelineDrawSystem(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderUI2D);
        SetRenderElementType("Text");
        AddDependency<PrimitiveRenderSystem>();
    }

    RenderPipelineBase* TextRenderSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Text");
    }

    void TextRenderSystem::OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd)
    {
        static_cast<TextRenderPipeline*>(pipeline)->RenderLayer(
            cmd, hgl::graph::mtl::RenderLayer::UI2D);
    }

}  // namespace hgl::ecs
