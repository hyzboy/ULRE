#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/type/RectScope.h>

namespace hgl::ecs
{
    class ECSContext;
    class Transform2DComponent;

    /**
     * Transform2DSystem -- Hierarchy2D 组的世界变换求值器
     *
     * 每帧（TickTransform 相位）遍历全部 Transform2DComponent：
     *   1. 找出全部根（parent 为空或父悬挂的孤儿节点）；
     *   2. 从根按「首子先序」DFS：world = parent.world * local（dirty 或父世界变才重算），
     *      同趟赋 tree_order（全局递增 DFS 序）并级联 effective_clip；
     *   3. 世界矩阵逐元素比对未变的分支截断重算传播（仍走完 DFS 以维护 tree_order/clip）。
     *
     * 无渲染副作用：结果由 CullAndSort2DSystem / 渲染 bridge（如文本）消费。
     */
    class Transform2DSystem : public System
    {
    private:
        ECSContext *world = nullptr;
        uint32_t tree_order_counter = 0;    // DFS 趟内全局递增

        void EvaluateNode(Transform2DComponent *node, bool parent_changed,
                          const math::RectScope2f *parent_child_clip);

    public:
        explicit Transform2DSystem(const AnsiString& name = "Transform2DSystem");
        ~Transform2DSystem() override = default;

        void SetWorld(ECSContext* w) { world = w; }
        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs