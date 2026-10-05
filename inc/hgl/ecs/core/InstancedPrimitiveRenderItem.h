#pragma once

#include <hgl/ecs/core/PrimitiveRenderItem.h>

namespace hgl::ecs
{
    /**
     * InstancedPrimitiveRenderItem - 多实例图元的 RenderItem
     *
     * A5b：图元多实例组件已删除 ⇒ 每实例状态（实例数 / 连续槽位 / GPU 绑定）
     * 改由该实体在世界 `MaterialRuntimeTable` 的 slot 承载；本项按实体现查，不缓存组件指针。
     */
    class InstancedPrimitiveRenderItem : public PrimitiveRenderItem
    {
    public:
        InstancedPrimitiveRenderItem(
            EntityID ent_id,
            const TransformAccessor &trans,
            MaterialRuntimeRowID mat_row = INVALID_MATERIAL_RUNTIME_ROW_ID,
            ECSContext *ctx = nullptr);

        ~InstancedPrimitiveRenderItem() override = default;

        uint32_t GetInstanceCount() const;
    };
}//namespace hgl::ecs
