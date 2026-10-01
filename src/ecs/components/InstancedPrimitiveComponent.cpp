#include <hgl/ecs/components/InstancedPrimitiveComponent.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/support/RenderItemDataStorage.h>

namespace hgl::ecs
{
    InstancedPrimitiveComponent::InstancedPrimitiveComponent(const std::string &name)
        : PrimitiveComponent(name)
    {
    }

    InstancedPrimitiveComponent::~InstancedPrimitiveComponent()
    {
        ReleaseInstances();
    }

    void InstancedPrimitiveComponent::OnDetach()
    {
        ReleaseInstances();
        PrimitiveComponent::OnDetach();
    }

    void InstancedPrimitiveComponent::SetInstanceCount(uint32_t count)
    {
        instance_count = count;
        if (instance_count > max_instances)
        {
            max_instances = instance_count;
        }
        if (allocated_instance_capacity < instance_count && count > 1)
        {
            AllocateContiguousInstances(instance_count);
        }
    }

    void InstancedPrimitiveComponent::SetMaxInstances(uint32_t max_count)
    {
        max_instances = max_count;
        if (allocated_instance_capacity < max_instances && max_instances > 1)
        {
            AllocateContiguousInstances(max_instances);
        }
    }

    bool InstancedPrimitiveComponent::AllocateContiguousInstances(uint32_t count)
    {
        if (count == 0)
            return false;

        // A5a：世界存储指针不再缓存于组件，每次经 owner 的 ECSContext 现取。
        RenderItemDataStorage *storage = owner_context
            ? owner_context->GetRenderItemStorage()
            : nullptr;

        if (!storage)
            return false;

        if (allocated_instance_capacity >= count && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
            return true;

        ReleaseInstances();

        render_item_handle = storage->AllocateContiguous(count);
        if (render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE)
            return false;

        allocated_instance_capacity = count;
        if (max_instances < count)
            max_instances = count;

        return true;
    }

    void InstancedPrimitiveComponent::ReleaseInstances()
    {
        RenderItemDataStorage *storage = owner_context
            ? owner_context->GetRenderItemStorage()
            : nullptr;

        if (storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            if (allocated_instance_capacity > 1)
            {
                storage->ReleaseContiguous(render_item_handle, allocated_instance_capacity);
            }
            else
            {
                storage->Release(render_item_handle);
            }
            render_item_handle = graph::INVALID_RENDER_ITEM_HANDLE;
            allocated_instance_capacity = 0;
        }
    }

    void InstancedPrimitiveComponent::EnsureRenderItemStorageAllocated()
    {
        const uint32_t target_count = max_instances > 0 ? max_instances : (instance_count > 0 ? instance_count : 1);
        if (target_count > 1)
        {
            if (allocated_instance_capacity < target_count)
            {
                AllocateContiguousInstances(target_count);
            }
        }
        else
        {
            PrimitiveComponent::EnsureRenderItemStorageAllocated();
        }
    }

    graph::RenderItemHandle InstancedPrimitiveComponent::GetInstanceHandle(uint32_t instance_idx) const
    {
        if (render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE || instance_idx >= allocated_instance_capacity)
            return graph::INVALID_RENDER_ITEM_HANDLE;

        return render_item_handle + instance_idx;
    }

    bool InstancedPrimitiveComponent::SetInstanceTransformID(uint32_t instance_idx, uint32_t transform_id)
    {
        const auto handle = GetInstanceHandle(instance_idx);
        RenderItemDataStorage *storage = owner_context ? owner_context->GetRenderItemStorage() : nullptr;
        if (handle == graph::INVALID_RENDER_ITEM_HANDLE || !storage)
            return false;

        return storage->SetTransformID(handle, transform_id);
    }

    bool InstancedPrimitiveComponent::SetInstanceGeometryID(uint32_t instance_idx, uint32_t geometry_id)
    {
        const auto handle = GetInstanceHandle(instance_idx);
        RenderItemDataStorage *storage = owner_context ? owner_context->GetRenderItemStorage() : nullptr;
        if (handle == graph::INVALID_RENDER_ITEM_HANDLE || !storage)
            return false;

        return storage->SetGeometryID(handle, geometry_id);
    }

    bool InstancedPrimitiveComponent::SetInstanceMaterialID(uint32_t instance_idx, uint32_t material_id)
    {
        const auto handle = GetInstanceHandle(instance_idx);
        RenderItemDataStorage *storage = owner_context ? owner_context->GetRenderItemStorage() : nullptr;
        if (handle == graph::INVALID_RENDER_ITEM_HANDLE || !storage)
            return false;

        return storage->SetMaterialID(handle, material_id);
    }

    bool InstancedPrimitiveComponent::SetInstanceTextureID(uint32_t instance_idx, uint32_t texture_id)
    {
        const auto handle = GetInstanceHandle(instance_idx);
        RenderItemDataStorage *storage = owner_context ? owner_context->GetRenderItemStorage() : nullptr;
        if (handle == graph::INVALID_RENDER_ITEM_HANDLE || !storage)
            return false;

        return storage->SetTextureID(handle, texture_id);
    }

    bool InstancedPrimitiveComponent::SetInstance4ID(uint32_t instance_idx, uint32_t transform_id, uint32_t geometry_id, uint32_t material_id, uint32_t texture_id)
    {
        const auto handle = GetInstanceHandle(instance_idx);
        RenderItemDataStorage *storage = owner_context ? owner_context->GetRenderItemStorage() : nullptr;
        if (handle == graph::INVALID_RENDER_ITEM_HANDLE || !storage)
            return false;

        return storage->Set4ID(handle, transform_id, geometry_id, material_id, texture_id);
    }

    bool InstancedPrimitiveComponent::SetAllInstances4ID(uint32_t base_transform_id, uint32_t geometry_id, uint32_t material_id, uint32_t texture_id, bool sequential_transforms)
    {
        RenderItemDataStorage *storage = owner_context ? owner_context->GetRenderItemStorage() : nullptr;
        if (render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE || !storage || allocated_instance_capacity == 0)
            return false;

        for (uint32_t i = 0; i < allocated_instance_capacity; ++i)
        {
            const uint32_t t_id = sequential_transforms ? (base_transform_id + i) : base_transform_id;
            storage->Set4ID(render_item_handle + i, t_id, geometry_id, material_id, texture_id);
        }
        return true;
    }
}//namespace hgl::ecs
