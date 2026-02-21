#include<hgl/ecs/systems/render/RenderPrimitiveSortSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveCullSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchSystem.h>
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    RenderPrimitiveSortSystem::RenderPrimitiveSortSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        AddDependency<RenderPrimitiveCullSystem>();
    }

    void RenderPrimitiveSortSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto batch_system = context->GetSystem<RenderPrimitiveBatchSystem>();
        if (!batch_system)
            return;

        if (!batch_system->PrepareFrame())
            return;

        batch_system->RunSorting();
    }
}
