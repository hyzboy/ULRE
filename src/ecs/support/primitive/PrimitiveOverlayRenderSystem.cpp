#include <hgl/ecs/support/primitive/PrimitiveOverlayRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    PrimitiveOverlayRenderSystem::PrimitiveOverlayRenderSystem(const std::string& name)
        : RenderPipelineDrawSystem(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderDebug);
        SetRenderElementType("Primitive");
    }

    RenderPipelineBase* PrimitiveOverlayRenderSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Primitive");
    }

    void PrimitiveOverlayRenderSystem::OnRender(RenderPipelineBase* /*pipeline*/, hgl::graph::RenderCmdBuffer* cmdBuffer)
    {
        PrimitiveRenderSystem::RenderBatches(context, cmdBuffer, true);
    }
}
