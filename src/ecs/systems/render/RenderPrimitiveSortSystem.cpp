#include<hgl/ecs/systems/render/RenderPrimitiveSortSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCullSystem.h>
#include<hgl/ecs/support/PrimitiveBatchPipeline.h>
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    RenderPrimitiveSortSystem::RenderPrimitiveSortSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch_RenderPrimitiveSortSystem);
        AddDependency<RenderPrimitiveCullSystem>();
    }

    void RenderPrimitiveSortSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto pipeline = context->GetPrimitiveBatchPipeline();
        if (!pipeline)
            return;

        if (!pipeline->PrepareFrame(context))
            return;

        pipeline->RunSorting();
    }
}
