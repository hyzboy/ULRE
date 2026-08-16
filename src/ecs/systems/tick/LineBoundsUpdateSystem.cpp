#include<hgl/ecs/systems/tick/LineBoundsUpdateSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/LinesComponent.h>
#include<hgl/ecs/components/BoundingBoxComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>

namespace hgl::ecs
{
    LineBoundsUpdateSystem::LineBoundsUpdateSystem(const std::string& name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::TickTransform);
        AddDependency<TransformSystem>();
    }

    void LineBoundsUpdateSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        std::vector<std::shared_ptr<LinesComponent>> line_components;
        world->GetComponents<LinesComponent>(line_components);

        for (const auto& line_comp : line_components)
        {
            if (!line_comp)
                continue;

            if (!line_comp->HasValidLocalBounds() && !line_comp->RecalculateLocalBounds())
                continue;

            Entity* owner = line_comp->GetOwner();
            if (!owner)
                continue;

            auto bbox = owner->GetComponent<BoundingBoxComponent>();
            if (!bbox)
            {
                bbox = owner->AddComponent<BoundingBoxComponent>();
            }

            if (!bbox)
                continue;

            const auto& local_bounds = line_comp->GetLocalBounds();
            bbox->SetAABB(local_bounds);

            auto transform = owner->GetComponent<TransformComponent>();
            if (transform)
            {
                const auto world_aabb = local_bounds.Transformed(transform->GetWorldMatrix());
                bbox->SetWorldAABB(world_aabb);
            }
            else
            {
                bbox->SetWorldAABB(local_bounds);
            }
        }
    }
}
