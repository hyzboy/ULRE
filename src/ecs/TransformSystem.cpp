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

        static_dirty = true;
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

        EnsureTransformBuffer();
        if (!transform_buffer)
            return;

        auto static_storage = TransformComponent::GetStaticStorage();
        auto dynamic_storage = TransformComponent::GetDynamicStorage();

        const uint32_t static_count = static_cast<uint32_t>(static_storage ? static_storage->GetSize() : 0);
        const uint32_t dynamic_count = static_cast<uint32_t>(dynamic_storage ? dynamic_storage->GetSize() : 0);

        if (static_count != last_static_count)
            static_dirty = true;

        transform_buffer->EnsureCapacity(static_count, dynamic_count, graph::BufferAllocPolicy::Auto);

        if (static_dirty && static_storage)
        {
            transform_buffer->WriteStaticFromStorage(*static_storage, static_count);
            static_dirty = false;
        }

        if (dynamic_storage)
        {
            transform_buffer->WriteDynamicFromStorage(*dynamic_storage, static_count, dynamic_count);
        }

        last_static_count = static_count;
        last_dynamic_count = dynamic_count;
    }

    void TransformSystem::EnsureTransformBuffer()
    {
        if (!transform_buffer && device)
        {
            transform_buffer = new ECSTransformAssignmentBuffer(device, ECSTransformAssignmentBuffer::Mode::MovableOnly);
            static_dirty = true;
        }
    }

    uint32_t TransformSystem::GetDynamicBaseIndex(const uint32_t static_count,const uint32_t dynamic_count) const
    {
        if (!transform_buffer)
            return static_count;

        return transform_buffer->GetDynamicBaseIndex(static_count, dynamic_count);
    }
}//namespace hgl::ecs
