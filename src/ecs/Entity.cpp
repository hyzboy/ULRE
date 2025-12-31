#include<hgl/ecs/Entity.h>

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
                pair.second->OnDetach();
            }
            components.clear();
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
