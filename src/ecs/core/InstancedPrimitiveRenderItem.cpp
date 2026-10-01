#include <hgl/ecs/core/InstancedPrimitiveRenderItem.h>
#include <hgl/ecs/support/MaterialRuntimeTable.h>

namespace hgl::ecs
{
    InstancedPrimitiveRenderItem::InstancedPrimitiveRenderItem(
        EntityID ent_id,
        const TransformAccessor &trans,
        MaterialRuntimeRowID mat_row,
        ECSContext *ctx)
        : PrimitiveRenderItem(ent_id, trans, mat_row, ctx)
    {
    }

    uint32_t InstancedPrimitiveRenderItem::GetInstanceCount() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return slot ? slot->instance_count : 0;
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetL2WBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return slot ? slot->l2w_buffer : nullptr;
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetL2WIndexBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return slot ? slot->l2w_index_buffer : nullptr;
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetMeshDrawParamsBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return slot ? slot->mesh_draw_params_buffer : nullptr;
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetMaterialDataRowsBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return slot ? slot->material_data_rows_buffer : nullptr;
    }

    hgl::graph::IndirectMeshTaskBuffer *InstancedPrimitiveRenderItem::GetIndirectMeshTaskBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return slot ? slot->indirect_cmds_buffer : nullptr;
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetIndirectCountBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return slot ? slot->indirect_count_buffer : nullptr;
    }

    uint64_t InstancedPrimitiveRenderItem::GetIndirectCountOffset() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return slot ? slot->indirect_count_offset : 0;
    }

    bool InstancedPrimitiveRenderItem::IsGPUDriven() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return slot ? slot->is_gpu_driven : false;
    }

    bool InstancedPrimitiveRenderItem::IsIndirect() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return slot ? slot->is_indirect : false;
    }
}//namespace hgl::ecs
