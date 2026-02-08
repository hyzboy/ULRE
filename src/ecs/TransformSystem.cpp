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

        RefreshHandleOrder();

        auto storage = TransformComponent::GetSharedStorage();
        const uint32_t static_count = GetStaticCount();
        const uint32_t dynamic_count = GetDynamicCount();

        if (static_count != last_static_count)
            static_dirty = true;

        transform_buffer->EnsureCapacity(static_count, dynamic_count, graph::BufferAllocPolicy::Auto);

        if (static_dirty && storage)
        {
            transform_buffer->WriteStaticFromHandles(*storage, static_handles);
            static_dirty = false;
        }

        if (storage)
        {
            transform_buffer->WriteDynamicFromHandles(*storage, static_count, dynamic_handles);
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

    bool TransformSystem::TryGetTransformGroupIndex(TransformDataStorage::HandleID handle, bool movable, uint32_t& out_index) const
    {
        if (handle == TransformDataStorage::INVALID_HANDLE)
            return false;

        if (movable)
        {
            auto it = dynamic_index_map.find(handle);
            if (it == dynamic_index_map.end())
                return false;
            out_index = it->second;
            return true;
        }

        auto it = static_index_map.find(handle);
        if (it == static_index_map.end())
            return false;
        out_index = it->second;
        return true;
    }

    void TransformSystem::RefreshHandleOrder()
    {
        static_handles.clear();
        dynamic_handles.clear();
        static_index_map.clear();
        dynamic_index_map.clear();

        if (!world)
            return;

        const auto& static_transforms = world->GetStaticTransforms();
        const auto& movable_transforms = world->GetMovableTransforms();

        static_handles.reserve(static_transforms.size());
        dynamic_handles.reserve(movable_transforms.size());

        for (const auto& weak_comp : static_transforms)
        {
            if (auto comp = weak_comp.lock())
            {
                const auto handle = comp->GetStorageHandle();
                if (handle == TransformDataStorage::INVALID_HANDLE)
                    continue;
                const uint32_t index = static_cast<uint32_t>(static_handles.size());
                static_handles.push_back(handle);
                static_index_map[handle] = index;
            }
        }

        for (const auto& weak_comp : movable_transforms)
        {
            if (auto comp = weak_comp.lock())
            {
                const auto handle = comp->GetStorageHandle();
                if (handle == TransformDataStorage::INVALID_HANDLE)
                    continue;
                const uint32_t index = static_cast<uint32_t>(dynamic_handles.size());
                dynamic_handles.push_back(handle);
                dynamic_index_map[handle] = index;
            }
        }
    }
}//namespace hgl::ecs
