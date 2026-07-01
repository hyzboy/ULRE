#include <hgl/ecs/support/primitive/PrimitiveSortSystem.h>
#include <hgl/ecs/support/primitive/PrimitiveRenderPipeline.h>
#include <hgl/ecs/support/primitive/PrimitiveCullSystem.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    PrimitiveSortSystem::PrimitiveSortSystem(const std::string& name)
        : SortSystem(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        SetRenderElementType("Primitive");
        AddDependency<PrimitiveCullSystem>();
    }

    RenderPipelineBase* PrimitiveSortSystem::GetPipeline(ECSContext* context)
    {
        return context->GetRenderPipeline("Primitive");
    }

    void PrimitiveSortSystem::OnSort(RenderPipelineBase* pipeline)
    {
        pipeline->RunSort();
    }
}
