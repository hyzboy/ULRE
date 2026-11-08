#pragma once

#include<hgl/ecs/core/entity/ComponentRow.h>
#include<hgl/type/RectScope.h>
#include<cstdint>

namespace hgl::ecs
{
    /**
     * Layer2DComponent -- 2D 渲染层与裁切声明（与 Transform2DComponent 搭配挂载）
     *
     * 排序键（CullAndSort2DSystem）：layer_id 主键 -> order_in_layer 次键 ->
     * Transform2D.tree_order 末键。同实体缺 Layer2D 时系统按零值层处理。
     *
     * 裁切：clip_children 为真时，本子树的 effective_clip 收窄为
     * 「父 clip 与本节点裁切区域的交集」；裁切区域 = clip_rect（局部坐标，非空时）
     * 或本节点 world_bounds（clip_rect 为空时）。由 Transform2DSystem 在
     * DFS 中求值写入 Transform2D 的世界缓存。
     */
    class Layer2DComponent : public ComponentRow
    {
    public:

        uint16_t layer_id = 0;            // 渲染层（大层压小层）
        uint16_t order_in_layer = 0;      // 层内序（与 tree_order 组合定最终顺序）
        bool     clip_children = false;   // 是否裁切子树
        math::RectScope2f clip_rect;      // 局部裁切矩形（空 = 用节点 world_bounds）

    public:

        Layer2DComponent() = default;

        ~Layer2DComponent() = default;

        uint16_t GetLayerID() const { return layer_id; }
        uint16_t GetOrderInLayer() const { return order_in_layer; }
        bool IsClipChildren() const { return clip_children; }
        const math::RectScope2f &GetClipRect() const { return clip_rect; }

        /// 层与层内序（CullAndSort2D 每帧全量重排，无需置脏）
        void SetLayer(uint16_t layer, uint16_t order)
        {
            layer_id = layer;
            order_in_layer = order;
        }

        void SetClipChildren(bool enable) { clip_children = enable; }

        /// 显式局部裁切区（传 nullptr 清除，回退到 world_bounds）
        void SetClipRect(const math::RectScope2f *rect)
        {
            if (rect) clip_rect = *rect;
            else      clip_rect = math::RectScope2f();
        }
    };//class Layer2DComponent

    /// 登记类型号（CPU-only：无 GPU 行、不写 EntityGPU.row[]）。
    template<> struct ComponentTypeOf<Layer2DComponent> { static constexpr ComponentType value = ComponentType::Layer2D; };
}//namespace hgl::ecs