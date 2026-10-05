#include <hgl/ecs/core/InstancedPrimitiveRenderItem.h>
#include <hgl/ecs/support/MaterialRuntimeTable.h>
#include <hgl/ecs/core/Context.h>

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

}//namespace hgl::ecs
