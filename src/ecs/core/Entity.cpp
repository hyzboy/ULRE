#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>

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

        void Entity::NotifyComponentAdded(const std::type_index& component_type)
        {
            if (context && id.IsValid())
            {
                context->NotifyComponentAdded(id, component_type);
            }
        }

        void Entity::NotifyComponentRemoved(const std::type_index& component_type)
        {
            if (context && id.IsValid())
            {
                context->NotifyComponentRemoved(id, component_type);
            }
        }

        void Entity::ReplaceComponent(size_t type_hash, const std::shared_ptr<Component>& component, const std::type_index& component_type)
        {
            if (!component)
                return;

            auto *existing = components.GetValuePointer(type_hash);
            if (existing && *existing)
            {
                NotifyComponentRemoved(component_type);
                UnregisterFromContext(type_hash, existing->get());
                (*existing)->OnDetach();
            }

            components[type_hash] = component;
            component->SetOwner(id, context);
            RegisterToContext(type_hash, component);
            component->OnAttach();
            NotifyComponentAdded(component_type);
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

        void Entity::AddComponentInstance(const std::shared_ptr<Component>& component)
        {
            if (!component)
                return;

            const size_t type_hash = typeid(*component).hash_code();
            ReplaceComponent(type_hash, component, std::type_index(typeid(*component)));
        }

        void Entity::DetachAllComponents(bool notify_systems)
        {
            for (auto& pair : components)
            {
                if (!pair.second)
                    continue;

                if (notify_systems)
                    NotifyComponentRemoved(std::type_index(typeid(*pair.second)));

                UnregisterFromContext(pair.first, pair.second.get());
                pair.second->OnDetach();
            }
            components.Clear();
        }
    }//namespace ecs
}//namespace hgl

