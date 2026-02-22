#include<hgl/ecs/systems/render/RenderPrimitiveBatchFinalizeSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchBuildSystem.h>
#include<hgl/ecs/support/PrimitiveBatchPipeline.h>
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    RenderPrimitiveBatchFinalizeSystem::RenderPrimitiveBatchFinalizeSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        SetRenderElementType("Primitive");
        AddDependency<RenderPrimitiveBatchBuildSystem>();
    }

    void RenderPrimitiveBatchFinalizeSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto pipeline = context->GetPrimitiveBatchPipeline();
        if (!pipeline)
            return;

        if (!pipeline->PrepareFrame(context))
            return;

        pipeline->RunBatching();
    }
}
