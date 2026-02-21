#include<hgl/ecs/systems/render/RenderPrimitiveBatchBuildSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveSortSystem.h>
#include<hgl/ecs/systems/render/RenderPrimitiveBatchSystem.h>
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

        auto batch_system = context->GetSystem<RenderPrimitiveBatchSystem>();
        if (!batch_system)
            return;

        if (!batch_system->PrepareFrame())
            return;

        batch_system->RunTransformIndexing();
    }
}
