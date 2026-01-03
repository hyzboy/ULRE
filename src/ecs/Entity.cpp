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

        void Entity::OnUpdate(float deltaTime)
        {
            // Update all components
            for (auto& pair : components)
            {
                pair.second->OnUpdate(deltaTime);
            }
        }
    }//namespace ecs
}//namespace hgl
