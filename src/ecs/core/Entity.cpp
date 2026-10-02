#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/log/Log.h>

namespace hgl
{
    namespace ecs
    {
        Entity::Entity(const std::string& name)
            : Object(name)
        {
        }

        Entity::~Entity()
        {
            DetachAllComponents(false);
        }

        void Entity::RegisterToContext(size_t type_hash, const std::shared_ptr<Component>& comp)
        {
            if(context && comp)
                context->RegisterComponentInstance(type_hash, comp);
        }

        void Entity::UnregisterFromContext(size_t type_hash, Component* comp_ptr)
        {
            if(context)
                context->UnregisterComponentInstance(type_hash, comp_ptr);
        }

        void Entity::MarkSceneDirty() const
        {
            if (context)
                context->MarkSceneStructureDirty();
        }

        void Entity::ReplaceComponent(size_t type_hash, const std::shared_ptr<Component>& component, const std::type_index& component_type, ComponentType slot)
        {
            if (!component)
                return;

            ComponentEntry *existing = FindComponentEntry(type_hash);
            if (existing && existing->component)
            {
                UnregisterFromContext(type_hash, existing->component.get());
                existing->component->OnDetach();
            }

            if (existing)
                existing->component = component;
            else
                components.push_back(ComponentEntry{type_hash, component});
            component->SetOwner(id, context);
            component->SetOwnerEntity(this);
            component->SetComponentSlot(slot);      // 槽位随挂载写入（单一写者）
            component_mask = ComponentMaskAdd(component_mask,slot);
            RegisterToContext(type_hash, component);
            component->OnAttach();
            MarkSceneDirty();
        }

        bool Entity::VerifyComponentMaskAgainstComponents() const
        {
            uint32_t expected = 0;

            for (const auto& entry : components)
                if (entry.component)
                    expected = ComponentMaskAdd(expected,entry.component->GetComponentSlot());

            return expected == component_mask;
        }

        void Entity::GetAllComponents(std::vector<std::shared_ptr<Component>>& out) const
        {
            out.clear();
            out.reserve(components.size());
            for (const auto& entry : components)
            {
                out.push_back(entry.component);
            }
        }


        Entity::ComponentEntry *Entity::FindComponentEntry(std::size_t type_hash)
        {
            for (auto& entry : components)
                if (entry.type_hash == type_hash)
                    return &entry;

            return nullptr;
        }

        const Entity::ComponentEntry *Entity::FindComponentEntry(std::size_t type_hash) const
        {
            for (const auto& entry : components)
                if (entry.type_hash == type_hash)
                    return &entry;

            return nullptr;
        }

        void Entity::EraseComponentEntry(std::size_t type_hash)
        {
            for (auto it = components.begin(); it != components.end(); ++it)
            {
                if (it->type_hash == type_hash)
                {
                    *it = components.back();        // 顺序无关 ⇒ 尾替（O(1)，不留空洞）
                    components.pop_back();
                    return;
                }
            }
        }

        void Entity::DetachAllComponents(bool notify_systems)
        {
#if ULRE_ECS_DEBUG_API
             GLogDebug("[Entity] DetachAllComponents entity='%s' id=(%u,%u) component_count=%zu notify=%d",
                     GetName().c_str(),
                     id.index,
                     id.generation,
                     components.size(),
                     notify_systems ? 1 : 0);
#endif

            for (auto& entry : components)
            {
                if (!entry.component)
                    continue;

#if ULRE_ECS_DEBUG_API
                GLogDebug("[Entity] Detach component entity='%s' comp='%s'",
                          GetName().c_str(),
                          entry.component->GetName().c_str());
#endif

                UnregisterFromContext(entry.type_hash, entry.component.get());
                entry.component->OnDetach();
            }
            components.clear();
            component_mask = 0;                     // 全卸 ⇒ 掩码归零
            MarkSceneDirty();
        }
    }//namespace ecs
}//namespace hgl

