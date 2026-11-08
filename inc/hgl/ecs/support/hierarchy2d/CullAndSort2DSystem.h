#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/type/RectScope.h>
#include<vector>

namespace hgl::ecs
{
    class Entity;
    class ECSContext;
    class Transform2DComponent;
    class Layer2DComponent;
    class Transform2DSystem;

    /**
     * DrawEntry2D -- CullAndSort2DSystem 输出的一个可见 2D 节点
     * 指针指向类型化池（地址稳定）；生命周期 = 到下一帧 Update 前。
     */
    struct DrawEntry2D
    {
        Entity *entity = nullptr;
        Transform2DComponent *transform = nullptr;
        const Layer2DComponent *layer = nullptr;  // 可为空（零值层）
        uint64_t sort_key = 0;
    };

    /**
     * CullAndSort2DSystem -- Hierarchy2D 的剔除 + 排序表生成器
     *
     * RenderBatch 相位（排在 Transform2DSystem 之后）。产出按 sort_key 升序
     * （layer_id << 48 | order_in_layer << 32 | tree_order）的可见节点表，
     * 供 2D 绘制消费方（bridge / 未来自建 2D 管线）直接顺序遍历。
     *
     * 视口剔除默认关闭（SetViewportRect 开启）：bounds 为空或不在视口内的
     * 叶子节点被剔除；有子节点的容器不因自身 bounds 被剔（子节点独立评估）。
     */
    class CullAndSort2DSystem : public System
    {
    private:
        ECSContext *world = nullptr;
        std::vector<DrawEntry2D> entries;

        math::RectScope2f viewport_rect;
        bool viewport_enabled = false;

    public:
        explicit CullAndSort2DSystem(const AnsiString& name = "CullAndSort2DSystem");
        ~CullAndSort2DSystem() override = default;

        void SetWorld(ECSContext* w) { world = w; }
        void Update(float deltaTime) override;

        /// 开启视口剔除并设定视口矩形（世界像素）
        void SetViewportRect(const math::RectScope2f &rect)
        {
            viewport_rect = rect;
            viewport_enabled = true;
        }

        void EnableViewportCull(bool enable) { viewport_enabled = enable; }

        /// 已按 sort_key 升序的可见节点表（本帧 Update 前有效）
        const std::vector<DrawEntry2D> &GetSortedEntries() const { return entries; }
    };
}//namespace hgl::ecs