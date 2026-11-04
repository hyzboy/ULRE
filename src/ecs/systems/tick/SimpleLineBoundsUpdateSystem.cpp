#include<hgl/ecs/systems/tick/SimpleLineBoundsUpdateSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/SimpleLinesComponent.h>
#include<hgl/ecs/support/cull/BoundingBoxAccessor.h>
#include<hgl/ecs/support/transform/TransformID.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>

namespace hgl::ecs
{
    SimpleLineBoundsUpdateSystem::SimpleLineBoundsUpdateSystem(const AnsiString& name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::TickTransform);
        AddDependency<TransformSystem>();
    }

    void SimpleLineBoundsUpdateSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        ValueArray<SimpleLinesComponent *> line_components;      // T2-c：池活跃集视图（裸指针）
        world->GetComponents<SimpleLinesComponent>(line_components);

        for (const auto& line_comp : line_components)
        {
            if (!line_comp)
                continue;

            if (!line_comp->HasValidLocalBounds() && !line_comp->RecalculateLocalBounds())
                continue;

            Entity* owner = line_comp->GetOwner();
            if (!owner)
                continue;

            const BoundingBoxAccessor bbox = world->GetOrCreateBoundingBox(owner->GetEntityID());

            if (!bbox.IsValid())
                continue;

            const auto& local_bounds = line_comp->GetLocalBounds();
            bbox.SetLocalBounds(local_bounds);

            const TransformAccessor transform = world->GetTransformByEntity(owner->GetEntityID());
            if (transform.IsValid())
            {
                const auto world_aabb = local_bounds.Transformed(transform.GetWorldMatrix());
                bbox.SetWorldBounds(world_aabb);
            }
            else
            {
                bbox.SetWorldBounds(local_bounds);
            }
        }
    }
}
