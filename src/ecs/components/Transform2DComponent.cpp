#include<hgl/ecs/components/Transform2DComponent.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<cmath>

namespace hgl::ecs
{
    // 取实体的 2D 变换组件（父悬挂/缺组件返回 nullptr ⇒ 调用方按孤儿处理）
    static inline Transform2DComponent *GetT2(Entity *e)
    {
        return e ? e->GetComponent<Transform2DComponent>() : nullptr;
    }

    void Transform2DComponent::MarkSubtreeDirty()
    {
        dirty = true;
        for (Entity *c = first_child; c; )
        {
            Transform2DComponent *ct = GetT2(c);
            if (!ct) break;
            c = ct->next_sibling;
            ct->MarkSubtreeDirty();
        }
    }

    bool Transform2DComponent::SetParent(Transform2DComponent *new_parent)
    {
        Entity *new_parent_entity = new_parent ? new_parent->GetOwner() : nullptr;

        if (new_parent_entity == parent_entity) return true;   // 已在目标位置

        if (new_parent)
        {
            if (new_parent == this) return false;

            // 防环：沿 new_parent 的祖先链上走，遇到自己 ⇒ 拒绝
            for (Entity *a = new_parent->parent_entity; a; )
            {
                if (a == owner) return false;
                Transform2DComponent *at = GetT2(a);
                if (!at) break;
                a = at->parent_entity;
            }
        }

        // 从旧父的子链中摘出
        if (Transform2DComponent *oldp = GetT2(parent_entity))
        {
            if (oldp->first_child == owner)
            {
                oldp->first_child = next_sibling;
            }
            else
            {
                Entity *prev = oldp->first_child;
                Transform2DComponent *pt = GetT2(prev);
                while (pt && pt->next_sibling != owner)
                {
                    prev = pt->next_sibling;
                    pt = GetT2(prev);
                }
                if (pt) pt->next_sibling = next_sibling;
            }
        }
        next_sibling = nullptr;

        // 挂到新父的链尾（保持登记顺序稳定）
        parent_entity = new_parent_entity;
        if (new_parent)
        {
            if (!new_parent->first_child)
            {
                new_parent->first_child = owner;
            }
            else
            {
                Entity *tail = new_parent->first_child;
                Transform2DComponent *tt = GetT2(tail);
                while (tt && tt->next_sibling)
                {
                    tail = tt->next_sibling;
                    tt = GetT2(tail);
                }
                if (tt) tt->next_sibling = owner;
                else parent_entity = nullptr;   // 兄弟链损坏（中间节点缺组件）：按根处理
            }
        }
        MarkSubtreeDirty();
        return true;
    }

    bool Transform2DComponent::AddChild(Transform2DComponent *child)
    {
        if (!child || child == this) return false;
        return child->SetParent(this);
    }

    bool Transform2DComponent::RecomputeWorld(const Transform2DComponent *parent)
    {
        const float c = std::cos(local_rotation);
        const float s = std::sin(local_rotation);
        const float sx = local_scale.x, sy = local_scale.y;

        // 仿射 A = T(pos) * R(rot) * S(scale) * T(-pivot*size)
        // 列主序 mat3：col0 = A·x 轴像，col1 = A·y 轴像，col2 = 平移
        const math::Vector2f ps = pivot * size;
        const math::Matrix3f local(
            c * sx,  s * sx,  0.0f,
           -s * sy,  c * sy,  0.0f,
            local_position.x - (c * sx * ps.x - s * sy * ps.y),
            local_position.y - (s * sx * ps.x + c * sy * ps.y),
            1.0f);

        const math::Matrix3f next = parent ? (parent->world_matrix * local) : local;

        const bool changed =
            next[0][0] != world_matrix[0][0] || next[0][1] != world_matrix[0][1] ||
            next[1][0] != world_matrix[1][0] || next[1][1] != world_matrix[1][1] ||
            next[2][0] != world_matrix[2][0] || next[2][1] != world_matrix[2][1];

        world_matrix = next;
        world_position = math::Vector2f(next[2][0], next[2][1]);

        // 局部矩形 [0,size] 四角变换 → 世界 AABB
        float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
        for (int i = 0; i < 4; ++i)
        {
            const float px = (i & 1) ? size.x : 0.0f;
            const float py = (i & 2) ? size.y : 0.0f;
            const float wx = next[0][0] * px + next[1][0] * py + next[2][0];
            const float wy = next[0][1] * px + next[1][1] * py + next[2][1];
            if (wx < minx) minx = wx;
            if (wx > maxx) maxx = wx;
            if (wy < miny) miny = wy;
            if (wy > maxy) maxy = wy;
        }
        if (size.x > 0.0f && size.y > 0.0f)
            world_bounds = math::RectScope2f(minx, miny, maxx - minx, maxy - miny);
        else
            world_bounds = math::RectScope2f();

        dirty = false;
        return changed;
    }
}//namespace hgl::ecs