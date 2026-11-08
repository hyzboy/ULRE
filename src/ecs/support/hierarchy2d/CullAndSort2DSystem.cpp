#include<hgl/ecs/support/hierarchy2d/CullAndSort2DSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/Transform2DComponent.h>
#include<hgl/ecs/components/Layer2DComponent.h>
#include<hgl/ecs/systems/tick/Transform2DSystem.h>
#include<algorithm>

namespace hgl::ecs
{
    CullAndSort2DSystem::CullAndSort2DSystem(const AnsiString& name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderBatch);
        AddDependency<Transform2DSystem>();
    }

    void CullAndSort2DSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        entries.clear();

        ValueArray<Transform2DComponent *> nodes;   // T2-c：池活跃集视图（裸指针）
        world->GetComponents<Transform2DComponent>(nodes);

        for (auto *node : nodes)
        {
            if (!node)
                continue;

            // -- 视口剔除（默认关闭）：只剔"无尺寸或出视口的叶子"，容器放行 --
            if (viewport_enabled)
            {
                const math::RectScope2f &b = node->world_bounds;
                const bool empty = (b.Width <= 0.0f || b.Height <= 0.0f);
                const bool outside = !empty &&
                    (b.GetRight() <= viewport_rect.GetLeft()  ||
                     b.GetLeft() >= viewport_rect.GetRight()  ||
                     b.GetBottom() <= viewport_rect.GetTop()  ||
                     b.GetTop() >= viewport_rect.GetBottom());
                if ((empty || outside) && !node->HasChildren())
                    continue;
            }

            DrawEntry2D entry;
            entry.entity = node->GetOwner();
            entry.transform = node;
            entry.layer = entry.entity ? entry.entity->GetComponent<Layer2DComponent>() : nullptr;

            const uint64_t layer_id = entry.layer ? entry.layer->layer_id : 0u;
            const uint64_t order_in_layer = entry.layer ? entry.layer->order_in_layer : 0u;
            entry.sort_key = (layer_id << 48) | (order_in_layer << 32) | node->tree_order;

            entries.push_back(entry);
        }

        std::sort(entries.begin(), entries.end(),
                  [](const DrawEntry2D &a, const DrawEntry2D &b)
                  {
                      return a.sort_key < b.sort_key;
                  });
    }
}//namespace hgl::ecs