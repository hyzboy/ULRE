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
                return 0;       // None / 越界：不占槽位（GetComponentRow(None) 恒 0）

            const uint32_t old = component_rows[index];

            component_rows[index] = row;      // A1(b)：唯一真源就是本表（没有第二份可失步 ⇒ 无对拍）

            return old;
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

            if (context)
                context->RefreshEntityRow(this);        // T3b：实体行表随全卸同步

            MarkSceneDirty();
        }
    }//namespace ecs
}//namespace hgl
