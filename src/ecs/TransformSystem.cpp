#include<hgl/ecs/TransformSystem.h>
#include"ECSTransformAssignmentBuffer.h"

namespace hgl::ecs
{
    TransformSystem::TransformSystem(const std::string& name)
        : System(name)
    {
    }

    void TransformSystem::Update(float deltaTime)
    {
        (void)deltaTime;

        if (!world || !updateMovable)
            return;

        const auto& movable_transforms = world->GetMovableTransforms();

        for (const auto& weak_comp : movable_transforms)
        {
            if (auto comp = weak_comp.lock())
            {
                comp->UpdateIfDirty();
            }
        }
    }

    void TransformSystem::UpdateStaticDirty()
    {
        if (!world)
            return;

        const auto& static_transforms = world->GetStaticTransforms();

        for (const auto& weak_comp : static_transforms)
        {
            if (auto comp = weak_comp.lock())
            {
                UpdateStaticTransformRecursive(comp);
            }
        }
    }

    void TransformSystem::UpdateStaticTransformRecursive(const std::shared_ptr<TransformComponent>& comp)
    {
        if (!comp)
            return;

        auto parent = comp->GetParent();
        if (parent)
        {
            auto parentTransform = parent->GetComponent<TransformComponent>();
            if (parentTransform && parentTransform->IsDirty())
            {
                UpdateStaticTransformRecursive(parentTransform);
            }
        }

        if (comp->IsDirty())
        {
            comp->UpdateIfDirty();
        }
    }

    void TransformSystem::SubmitTransformUpdates()
    {
        if (!world)
            return;

        ECSTransformAssignmentBuffer::FlushAllPendingUpdates();
    }
}//namespace hgl::ecs
