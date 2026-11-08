#include<hgl/ecs/systems/tick/Transform2DSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/Transform2DComponent.h>
#include<hgl/ecs/components/Layer2DComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>

namespace hgl::ecs
{
    static inline Transform2DComponent *GetT2(Entity *e)
    {
        return e ? e->GetComponent<Transform2DComponent>() : nullptr;
    }

    // 矩形交集；任一为空（w/h<=0）或无重叠 => 空矩形
    static inline math::RectScope2f IntersectRect(const math::RectScope2f &a, const math::RectScope2f &b)
    {
        if (a.Width <= 0.0f || a.Height <= 0.0f || b.Width <= 0.0f || b.Height <= 0.0f)
            return math::RectScope2f();

        const float l = hgl_max(a.GetLeft(), b.GetLeft());
        const float t = hgl_max(a.GetTop(), b.GetTop());
        const float r = hgl_min(a.GetRight(), b.GetRight());
        const float bt = hgl_min(a.GetBottom(), b.GetBottom());

        if (r <= l || bt <= t) return math::RectScope2f();
        return math::RectScope2f(l, t, r - l, bt - t);
    }

    // 局部矩形经仿射矩阵变换后的世界 AABB
    static inline math::RectScope2f TransformRect(const math::Matrix3f &m, const math::RectScope2f &rc)
    {
        float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
        for (int i = 0; i < 4; ++i)
        {
            const float px = (i & 1) ? rc.GetRight()  : rc.GetLeft();
            const float py = (i & 2) ? rc.GetBottom() : rc.GetTop();
            const float wx = m[0][0] * px + m[1][0] * py + m[2][0];
            const float wy = m[0][1] * px + m[1][1] * py + m[2][1];
            if (wx < minx) minx = wx;
            if (wx > maxx) maxx = wx;
            if (wy < miny) miny = wy;
            if (wy > maxy) maxy = wy;
        }
        return math::RectScope2f(minx, miny, maxx - minx, maxy - miny);
    }

    Transform2DSystem::Transform2DSystem(const AnsiString& name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::TickTransform);
        AddDependency<TransformSystem>();
    }

    void Transform2DSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        ValueArray<Transform2DComponent *> nodes;   // T2-c：池活跃集视图（裸指针）
        world->GetComponents<Transform2DComponent>(nodes);

        tree_order_counter = 0;

        for (auto *node : nodes)
        {
            if (!node)
                continue;

            // 根 & 孤儿（父实体缺 Transform2D 组件）=> 均按根求值
            if (node->parent_entity == nullptr || GetT2(node->parent_entity) == nullptr)
                EvaluateNode(node, false, nullptr);
        }
    }

    void Transform2DSystem::EvaluateNode(Transform2DComponent *node, bool parent_changed,
                                         const math::RectScope2f *parent_child_clip)
    {
        node->tree_order = tree_order_counter++;

        // -- 世界求值：dirty 或父世界已变才重算（逐元素比对可截断传播） --
        bool changed = false;
        if (node->dirty || parent_changed)
        {
            Transform2DComponent *pt = GetT2(node->parent_entity);
            changed = node->RecomputeWorld(pt);
        }
        else
        {
            node->dirty = false;
        }

        // -- 裁切级联：本节点受父 clip 约束；clip_children 时子树 clip 收窄 --
        node->SetInheritedClip(parent_child_clip);

        bool has_child_clip = (parent_child_clip != nullptr);
        math::RectScope2f child_clip = parent_child_clip ? *parent_child_clip : math::RectScope2f();

        if (Entity *owner = node->GetOwner())
        {
            if (const Layer2DComponent *layer = owner->GetComponent<Layer2DComponent>())
            {
                if (layer->clip_children)
                {
                    math::RectScope2f region;
                    if (layer->clip_rect.Width > 0.0f && layer->clip_rect.Height > 0.0f)
                        region = TransformRect(node->world_matrix, layer->clip_rect);
                    else
                        region = node->world_bounds;

                    child_clip = has_child_clip ? IntersectRect(child_clip, region) : region;
                    has_child_clip = true;
                }
            }
        }

        // -- 子节点先序 DFS（首子先出，兄弟按挂接顺序） --
        for (Entity *c = node->first_child; c; )
        {
            Transform2DComponent *ct = GetT2(c);
            if (!ct) break;
            c = ct->next_sibling;
            EvaluateNode(ct, changed, has_child_clip ? &child_clip : nullptr);
        }
    }
}//namespace hgl::ecs