#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/log/Log.h>

namespace hgl
{
    namespace ecs
    {
        Entity::Entity(const AnsiString& name)
            : Object(name)
        {
        }

        Entity::~Entity()
        {
            DetachAllComponents();

            // T3b：实体行表清 `Active`（实体已死；其余字段保留供诊断）——
            // 实体生死是**单一入口**：所有销毁路径（DestroyEntity / Clear / 世界 Shutdown）
            // 都会走到显式 `~Entity()`（T2-a 块池的约定）。
            if (context)
                context->DeactivateEntityRow(id);
        }

        void Entity::MarkSceneDirty() const
        {
            if (context)
                context->MarkSceneStructureDirty();
        }

        uint32_t Entity::SetComponentRow(ComponentType type, uint32_t row)
        {
            const uint32_t index = static_cast<uint32_t>(type);

            if (index == 0 || index >= COMPONENT_TYPE_COUNT)
                return 0;       // None / 越界：不占槽位（`HasComponentType(None)` 恒假）

            const uint32_t old = component_rows[index];

            component_rows[index] = row;

            if (row)
                component_mask |= ComponentTypeBit(type);
            else
                component_mask &= ~ComponentTypeBit(type);

            return old;
        }

        bool Entity::VerifyComponentMaskAgainstComponents() const
        {
            uint32_t expected = 0;

            for (uint32_t i = 1; i < COMPONENT_TYPE_COUNT; ++i)
                if (component_rows[i])
                    expected |= (1u << i);

            return expected == component_mask;
        }

        void Entity::DetachAllComponents()
        {
            for (uint32_t i = 1; i < COMPONENT_TYPE_COUNT; ++i)
            {
                const uint32_t row = component_rows[i];

                if (!row)
                    continue;

                component_rows[i] = 0;

                if (context)
                    context->DestroyComponent(static_cast<ComponentType>(i), row);
            }

            component_mask = 0;

            if (context)
                context->RefreshEntityRow(this);        // T3b：实体行表随全卸同步

            MarkSceneDirty();
        }
    }//namespace ecs
}//namespace hgl
