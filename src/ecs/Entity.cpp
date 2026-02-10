#include<hgl/ecs/Entity.h>
#include<hgl/ecs/Context.h>

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
            // Detach all components before destruction
            for (auto& pair : components)
            {
                UnregisterFromContext(pair.first, pair.second.get());
                pair.second->OnDetach();
            }
            components.clear();
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
            out.reserve(components.size());
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
            components[type_hash] = component;
            component->SetOwner(id, context);
            RegisterToContext(type_hash, component);
            component->OnAttach();
            NotifyComponentAdded(std::type_index(typeid(*component)));
        }
    }//namespace ecs
}//namespace hgl
