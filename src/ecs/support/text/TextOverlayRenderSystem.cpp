#include <hgl/ecs/support/text/TextOverlayRenderSystem.h>
#include <hgl/ecs/support/text/TextRenderPipeline.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/mtl/RenderLayer.h>

namespace hgl::ecs
{
    TextOverlayRenderSystem::TextOverlayRenderSystem(const std::string& name)
        : RenderPipelineDrawSystem(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderOverlay);
        SetRenderElementType("Text");
    }

    RenderPipelineBase* TextOverlayRenderSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Text");
    }

    void TextOverlayRenderSystem::OnRender(RenderPipelineBase* pipeline, hgl::graph::RenderCmdBuffer* cmd)
    {
        static_cast<TextRenderPipeline*>(pipeline)->RenderLayer(
            cmd, hgl::graph::mtl::RenderLayer::Overlay);
    }

}  // namespace hgl::ecs
