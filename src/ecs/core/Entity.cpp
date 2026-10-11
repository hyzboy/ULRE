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

        #ifndef NDEBUG
            // §2 #1（2026-10-11）：debug-only 对拍。掩码是 `component_rows` 的**缓存视图**，
            // 生产期此前没有任何一致性门（对拍器只在 `TestComponentMask` 里被调用）。
            // 放在写完掩码**之后**做全表对照：O(COMPONENT_TYPE_COUNT) 的循环，Release（NDEBUG）零成本。
            if (!VerifyComponentMaskAgainstComponents())
                GLogError(u8"[Entity] component_mask 与 component_rows 失步（掩码缓存视图写坏）："
                          u8"type=%u row=%u mask=0x%08x",
                          static_cast<uint32_t>(type), row, component_mask);
        #endif//NDEBUG

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

        #ifndef NDEBUG
            // §2 #1：同上，全卸路径也要对拍（此处应恒一致）
            if (!VerifyComponentMaskAgainstComponents())
                GLogError(u8"[Entity] DetachAllComponents 后 component_mask 与 component_rows 失步");
        #endif//NDEBUG

            if (context)
                context->RefreshEntityRow(this);        // T3b：实体行表随全卸同步

            MarkSceneDirty();
        }
    }//namespace ecs
}//namespace hgl
