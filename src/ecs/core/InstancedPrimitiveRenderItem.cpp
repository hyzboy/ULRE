#include <hgl/ecs/core/InstancedPrimitiveRenderItem.h>
#include <hgl/ecs/support/MaterialRuntimeTable.h>
#include <hgl/ecs/support/MaterialRuntimeBufferTable.h>
#include <hgl/ecs/core/Context.h>               // 经世界取句柄表（Resolve 需要完整 ECSContext）
#include <hgl/vk/buffer/BufferOwner.h>          // 句柄表按 BufferOwner* 存：downcast 需要完整类型
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/vk/buffer/IndirectCommandBuffer.h>

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

    namespace
    {
        /// B5-a：slot 里的缓冲区是**句柄**，取指针要走世界级句柄表
        template<typename T>
        T *ResolveSlotBuffer(const ECSContext *ctx, const uint32_t handle)
        {
            if (!ctx || handle == MaterialRuntimeBufferTable::kInvalidBufferHandle)
                return nullptr;

            return ctx->GetMaterialRuntimeBufferTable().Resolve<T>(handle);
        }
    }//namespace

    uint32_t InstancedPrimitiveRenderItem::GetInstanceCount() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return slot ? slot->instance_count : 0;
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetL2WBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return ResolveSlotBuffer<hgl::graph::DeviceBuffer>(context, slot ? slot->l2w_buffer_handle : 0);
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetL2WIndexBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return ResolveSlotBuffer<hgl::graph::DeviceBuffer>(context, slot ? slot->l2w_index_buffer_handle : 0);
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetMeshDrawParamsBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return ResolveSlotBuffer<hgl::graph::DeviceBuffer>(context, slot ? slot->mesh_draw_params_buffer_handle : 0);
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetMaterialDataRowsBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return ResolveSlotBuffer<hgl::graph::DeviceBuffer>(context, slot ? slot->material_data_rows_buffer_handle : 0);
    }

    hgl::graph::IndirectMeshTaskBuffer *InstancedPrimitiveRenderItem::GetIndirectMeshTaskBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return ResolveSlotBuffer<hgl::graph::IndirectMeshTaskBuffer>(
            context, slot ? slot->indirect_cmds_buffer_handle : 0);
    }

    hgl::graph::DeviceBuffer *InstancedPrimitiveRenderItem::GetIndirectCountBuffer() const
    {
        const MaterialRuntimeSlot *slot = GetRuntimeSlot();
        return ResolveSlotBuffer<hgl::graph::DeviceBuffer>(
            context, slot ? slot->indirect_count_buffer_handle : 0);
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
