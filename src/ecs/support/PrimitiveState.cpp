#include<hgl/ecs/support/PrimitiveState.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/MaterialData.h>
#include<hgl/ecs/support/MaterialRuntimeTable.h>
#include<hgl/ecs/support/RenderItemDataStorage.h>

namespace hgl::ecs
{
    // ── 能力判定 ──

    bool CanRender(const Entity *entity)
    {
        if (!entity)
            return false;

        ECSContext *context = entity->GetContext();
        if (!context)
            return false;

        const GeometryData *geometry = context->GetGeometryData(entity->GetEntityID());

        return geometry && geometry->GetPrimitiveAsset() != nullptr;
    }

    bool HasAnyMaterialSource(const Entity *entity)
    {
        if (!entity)
            return false;

        ECSContext *context = entity->GetContext();
        if (!context)
            return false;

        const EntityID id = entity->GetEntityID();
        const MaterialData *material_data = context->GetMaterialData(id);
        const GeometryData *geometry = context->GetGeometryData(id);

        return (material_data && material_data->HasRecipeOverride())
            || (geometry && geometry->GetAssetMaterialRecipe() != nullptr);
    }

    // ── render_item 4-ID 绑定 ──

    graph::RenderItemHandle EnsureRenderItemHandle(ECSContext &world, EntityID owner)
    {
        RenderItemDataStorage *storage = world.GetRenderItemStorage();
        if (!storage)
            return graph::INVALID_RENDER_ITEM_HANDLE;

        MaterialRuntimeSlot &slot = world.GetOrCreateMaterialRuntimeSlot(owner);

        if (slot.render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE)
            slot.render_item_handle = storage->Allocate();

        return slot.render_item_handle;
    }

    graph::RenderItemHandle GetRenderItemHandle(const ECSContext &world, EntityID owner)
    {
        const MaterialRuntimeTable *runtime_table = world.GetMaterialRuntimeTable();
        const MaterialRuntimeSlot *slot = runtime_table ? runtime_table->GetSlot(owner) : nullptr;

        return slot ? slot->render_item_handle : graph::INVALID_RENDER_ITEM_HANDLE;
    }

    bool SetRenderItem4ID(ECSContext &world, EntityID owner,
                          uint32_t transform_id, uint32_t geometry_id,
                          uint32_t material_id, uint32_t texture_id)
    {
        const graph::RenderItemHandle handle = EnsureRenderItemHandle(world, owner);
        if (handle == graph::INVALID_RENDER_ITEM_HANDLE)
            return false;

        RenderItemDataStorage *storage = world.GetRenderItemStorage();
        return storage
            && storage->Set4ID(handle, transform_id, geometry_id, material_id, texture_id);
    }

    void ReleaseRenderItemHandle(ECSContext &world, EntityID owner)
    {
        MaterialRuntimeTable *runtime_table = world.GetMaterialRuntimeTable();
        MaterialRuntimeSlot *slot = runtime_table ? runtime_table->GetSlot(owner) : nullptr;
        if (!slot)
            return;

        RenderItemDataStorage *storage = world.GetRenderItemStorage();

        if (storage && slot->render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            if (slot->allocated_instance_capacity > 1)
                storage->ReleaseContiguous(slot->render_item_handle,
                                           slot->allocated_instance_capacity);
            else
                storage->Release(slot->render_item_handle);
        }

        slot->render_item_handle = graph::INVALID_RENDER_ITEM_HANDLE;
        slot->allocated_instance_capacity = 0;
    }

    // ── 多实例连续槽位 ──

    bool AllocateContiguousInstances(ECSContext &world, EntityID owner, uint32_t count)
    {
        if (count == 0)
            return false;

        RenderItemDataStorage *storage = world.GetRenderItemStorage();
        if (!storage)
            return false;

        MaterialRuntimeSlot &slot = world.GetOrCreateMaterialRuntimeSlot(owner);

        if (slot.allocated_instance_capacity >= count
         && slot.render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
            return true;

        ReleaseInstances(world, owner);

        slot.render_item_handle = storage->AllocateContiguous(count);
        if (slot.render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE)
            return false;

        slot.allocated_instance_capacity = count;
        if (slot.max_instances < count)
            slot.max_instances = count;

        return true;
    }

    void ReleaseInstances(ECSContext &world, EntityID owner)
    {
        MaterialRuntimeTable *runtime_table = world.GetMaterialRuntimeTable();
        MaterialRuntimeSlot *slot = runtime_table ? runtime_table->GetSlot(owner) : nullptr;
        if (!slot)
            return;

        RenderItemDataStorage *storage = world.GetRenderItemStorage();

        if (storage && slot->render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            if (slot->allocated_instance_capacity > 1)
                storage->ReleaseContiguous(slot->render_item_handle,
                                           slot->allocated_instance_capacity);
            else
                storage->Release(slot->render_item_handle);

            slot->render_item_handle = graph::INVALID_RENDER_ITEM_HANDLE;
            slot->allocated_instance_capacity = 0;
        }
    }

    uint32_t GetAllocatedInstanceCapacity(const ECSContext &world, EntityID owner)
    {
        const MaterialRuntimeTable *runtime_table = world.GetMaterialRuntimeTable();
        const MaterialRuntimeSlot *slot = runtime_table ? runtime_table->GetSlot(owner) : nullptr;

        return slot ? slot->allocated_instance_capacity : 0;
    }

    uint32_t GetInstanceCount(const ECSContext &world, EntityID owner)
    {
        const MaterialRuntimeTable *runtime_table = world.GetMaterialRuntimeTable();
        const MaterialRuntimeSlot *slot = runtime_table ? runtime_table->GetSlot(owner) : nullptr;

        return slot ? slot->instance_count : 0;
    }

    void SetInstanceCount(ECSContext &world, EntityID owner, uint32_t count)
    {
        MaterialRuntimeSlot &slot = world.GetOrCreateMaterialRuntimeSlot(owner);

        slot.instance_count = count;
        if (slot.instance_count > slot.max_instances)
            slot.max_instances = slot.instance_count;

        if (slot.allocated_instance_capacity < slot.instance_count && count > 1)
            AllocateContiguousInstances(world, owner, slot.instance_count);
    }

    void SetMaxInstances(ECSContext &world, EntityID owner, uint32_t max_count)
    {
        MaterialRuntimeSlot &slot = world.GetOrCreateMaterialRuntimeSlot(owner);

        slot.max_instances = max_count;
        if (slot.allocated_instance_capacity < slot.max_instances && slot.max_instances > 1)
            AllocateContiguousInstances(world, owner, slot.max_instances);
    }

    graph::RenderItemHandle GetInstanceHandle(const ECSContext &world, EntityID owner, uint32_t instance_idx)
    {
        const MaterialRuntimeTable *runtime_table = world.GetMaterialRuntimeTable();
        const MaterialRuntimeSlot *slot = runtime_table ? runtime_table->GetSlot(owner) : nullptr;

        if (!slot
         || slot->render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE
         || instance_idx >= slot->allocated_instance_capacity)
            return graph::INVALID_RENDER_ITEM_HANDLE;

        return slot->render_item_handle + instance_idx;
    }

    bool SetInstance4ID(ECSContext &world, EntityID owner, uint32_t instance_idx,
                        uint32_t transform_id, uint32_t geometry_id,
                        uint32_t material_id, uint32_t texture_id)
    {
        const graph::RenderItemHandle handle = GetInstanceHandle(world, owner, instance_idx);
        if (handle == graph::INVALID_RENDER_ITEM_HANDLE)
            return false;

        RenderItemDataStorage *storage = world.GetRenderItemStorage();
        return storage
            && storage->Set4ID(handle, transform_id, geometry_id, material_id, texture_id);
    }

    bool SetAllInstances4ID(ECSContext &world, EntityID owner,
                            uint32_t base_transform_id, uint32_t geometry_id,
                            uint32_t material_id, uint32_t texture_id,
                            bool sequential_transforms)
    {
        const MaterialRuntimeTable *runtime_table = world.GetMaterialRuntimeTable();
        const MaterialRuntimeSlot *slot = runtime_table ? runtime_table->GetSlot(owner) : nullptr;

        RenderItemDataStorage *storage = world.GetRenderItemStorage();
        if (!slot || !storage
         || slot->render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE
         || slot->allocated_instance_capacity == 0)
            return false;

        for (uint32_t i = 0; i < slot->allocated_instance_capacity; ++i)
        {
            const uint32_t t_id = sequential_transforms ? (base_transform_id + i) : base_transform_id;
            storage->Set4ID(slot->render_item_handle + i, t_id, geometry_id, material_id, texture_id);
        }
        return true;
    }

    // ── 多实例 GPU 绑定 ──

    void SetL2WBuffer(ECSContext &world, EntityID owner, graph::DeviceBuffer *buf)
    {
        world.GetOrCreateMaterialRuntimeSlot(owner).l2w_buffer = buf;
    }

    void SetL2WIndexBuffer(ECSContext &world, EntityID owner, graph::DeviceBuffer *buf)
    {
        world.GetOrCreateMaterialRuntimeSlot(owner).l2w_index_buffer = buf;
    }

    void SetMeshDrawParamsBuffer(ECSContext &world, EntityID owner, graph::DeviceBuffer *buf)
    {
        world.GetOrCreateMaterialRuntimeSlot(owner).mesh_draw_params_buffer = buf;
    }

    void SetMaterialDataRowsBuffer(ECSContext &world, EntityID owner, graph::DeviceBuffer *buf)
    {
        world.GetOrCreateMaterialRuntimeSlot(owner).material_data_rows_buffer = buf;
    }

    void SetIndirectMeshTaskBuffer(ECSContext &world, EntityID owner, graph::IndirectMeshTaskBuffer *buf)
    {
        MaterialRuntimeSlot &slot = world.GetOrCreateMaterialRuntimeSlot(owner);

        slot.indirect_cmds_buffer = buf;
        if (buf)
            slot.is_indirect = true;
    }

    void SetIndirectCountBuffer(ECSContext &world, EntityID owner, graph::DeviceBuffer *buf, uint64_t offset)
    {
        MaterialRuntimeSlot &slot = world.GetOrCreateMaterialRuntimeSlot(owner);

        slot.indirect_count_buffer = buf;
        slot.indirect_count_offset = offset;
    }

    void SetGPUDriven(ECSContext &world, EntityID owner, bool enabled)
    {
        world.GetOrCreateMaterialRuntimeSlot(owner).is_gpu_driven = enabled;
    }
}//namespace hgl::ecs
