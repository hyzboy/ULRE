#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/core/ECSCoreContext.h>
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
        }

        void Entity::SetContext(ECSCoreContext *ctx)
        {
            context = ctx;
        }

        void Entity::MarkSceneDirty() const
        {
            if (context)
                context->NotifySceneStructureDirty();
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
            if (context && !context->CanMutateEntities())
                return;

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
                context->NotifyEntityRowsChanged(this);

            MarkSceneDirty();
        }
    }//namespace ecs
}//namespace hgl
