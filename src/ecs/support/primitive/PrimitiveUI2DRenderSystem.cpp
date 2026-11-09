#include <hgl/ecs/support/primitive/PrimitiveUI2DRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    PrimitiveUI2DRenderSystem::PrimitiveUI2DRenderSystem(const std::string& name)
        : RenderPipelineDrawSystem(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderUI2D);
        SetRenderElementType("Primitive");
    }

    RenderPipelineBase* PrimitiveUI2DRenderSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Primitive");
    }

    void PrimitiveUI2DRenderSystem::OnRender(RenderPipelineBase* /*pipeline*/, hgl::graph::RenderCmdBuffer* cmdBuffer)
    {
        PrimitiveRenderSystem::RenderBatches(context, cmdBuffer, hgl::graph::mtl::RenderLayer::UI2D);
    }
}
