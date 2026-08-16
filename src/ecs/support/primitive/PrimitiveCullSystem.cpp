#include <hgl/ecs/support/primitive/PrimitiveCullSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    PrimitiveCullSystem::PrimitiveCullSystem(const std::string& name)
        : CullSystem(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderCollect);
        SetRenderElementType("Primitive");
    }

    RenderPipelineBase* PrimitiveCullSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Primitive");
    }

    void PrimitiveCullSystem::OnCull(RenderPipelineBase* pipeline)
    {
        pipeline->RunCull();
    }
}
