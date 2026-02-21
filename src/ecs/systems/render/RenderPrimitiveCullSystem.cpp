#include<hgl/ecs/systems/render/RenderPrimitiveCullSystem.h>
#include<hgl/ecs/support/PrimitiveBatchPipeline.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    RenderPrimitiveCullSystem::RenderPrimitiveCullSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect_RenderPrimitiveCullSystem);
        AddDependency<RenderPrimitiveCollectSystem>();
    }

    void RenderPrimitiveCullSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto pipeline = context->GetPrimitiveBatchPipeline();
        if (!pipeline)
            return;

        if (!pipeline->PrepareFrame(context))
            return;

        pipeline->RunCulling();
    }
}
