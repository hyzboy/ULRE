#include<hgl/ecs/systems/render/RenderPrimitiveBatchFinalizeSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchBuildSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchSystem.h>
#include<hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    RenderPrimitiveBatchFinalizeSystem::RenderPrimitiveBatchFinalizeSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderBatch);
        SetExecutionOrder(ExecutionPhase::RenderBatch);
        AddDependency<RenderPrimitiveBatchBuildSystem>();
    }

    void RenderPrimitiveBatchFinalizeSystem::Update(float /*deltaTime*/)
    {
        if (!context)
            return;

        auto batch_system = context->GetSystem<RenderPrimitiveBatchSystem>();
        if (!batch_system)
            return;

        if (!batch_system->PrepareFrame())
            return;

        batch_system->RunBatching();
    }
}
