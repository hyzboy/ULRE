#include<hgl/ecs/systems/render/RenderPrimitiveCullSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    RenderPrimitiveCullSystem::RenderPrimitiveCullSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect);
        AddDependency<RenderPrimitiveCollectSystem>();
    }

    void RenderPrimitiveCullSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto batch_system = context->GetSystem<RenderPrimitiveBatchSystem>();
        if (!batch_system)
            return;

        if (!batch_system->PrepareFrame())
            return;

        batch_system->RunCulling();
    }
}
