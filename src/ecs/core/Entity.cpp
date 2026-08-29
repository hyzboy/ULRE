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

        void Entity::ReplaceComponent(size_t type_hash, const std::shared_ptr<Component>& component, const std::type_index& component_type)
        {
            if (!component)
                return;

            auto *existing = components.GetValuePointer(type_hash);
            if (existing && *existing)
            {
                UnregisterFromContext(type_hash, existing->get());
                (*existing)->OnDetach();
            }

            components[type_hash] = component;
            component->SetOwner(id, context);
            RegisterToContext(type_hash, component);
            component->OnAttach();
            MarkSceneDirty();
        }

        void Entity::OnUpdate(float deltaTime)
        {
            // Update all components
            for (auto& pair : components)
            {
                pair.second->OnUpdate(deltaTime);
            }
        }

        void Entity::GetAllComponents(std::vector<std::shared_ptr<Component>>& out) const
        {
            out.clear();
            out.reserve(static_cast<size_t>(components.GetCount()));
            for (const auto& pair : components)
            {
                out.push_back(pair.second);
            }
        }


        void Entity::DetachAllComponents(bool notify_systems)
        {
#if ULRE_ECS_DEBUG_API
             GLogDebug("[Entity] DetachAllComponents entity='%s' id=(%u,%u) component_count=%zu notify=%d",
                     GetName().c_str(),
                     id.index,
                     id.generation,
                     static_cast<size_t>(components.GetCount()),
                     notify_systems ? 1 : 0);
#endif

            for (auto& pair : components)
            {
                if (!pair.second)
                    continue;

#if ULRE_ECS_DEBUG_API
                GLogDebug("[Entity] Detach component entity='%s' comp='%s'",
                          GetName().c_str(),
                          pair.second->GetName().c_str());
#endif

                UnregisterFromContext(pair.first, pair.second.get());
                pair.second->OnDetach();
            }
            components.Clear();
            MarkSceneDirty();
        }
    }//namespace ecs
}//namespace hgl

