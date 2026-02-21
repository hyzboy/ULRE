#include<hgl/ecs/systems/render/RenderPrimitiveBatchBuildSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSortSystem.h>
#include<hgl/ecs/support/PrimitiveBatchPipeline.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    RenderPrimitiveBatchBuildSystem::RenderPrimitiveBatchBuildSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        AddDependency<RenderPrimitiveSortSystem>();
        AddDependency<TransformSystem>();
    }

    void RenderPrimitiveBatchBuildSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto pipeline = context->GetPrimitiveBatchPipeline();
        if (!pipeline)
            return;

        if (!pipeline->PrepareFrame(context))
            return;

        pipeline->RunTransformIndexing();
    }
}
