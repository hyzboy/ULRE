#include<hgl/ecs/World.h>

namespace hgl
{
    namespace ecs
    {
        World::World(const std::string& name)
            : Object(name)
            , active(false)
        {
        }

        World::~World()
        {
            Shutdown();
        }

        void World::Initialize()
        {
            // Initialize all systems
            for (auto& pair : systems)
            {
                pair.second->Initialize();
            }

            active = true;
            OnCreate();
        }

        void World::Shutdown()
        {
            if (!active)
                return;

            // Destroy all entities
            for (auto& entity : entities)
            {
                entity->OnDestroy();
            }
            entities.clear();

            // Shutdown all systems
            for (auto& pair : systems)
            {
                pair.second->Shutdown();
            }
            systems.clear();

            active = false;
            OnDestroy();
        }

        void World::Update(float deltaTime)
        {
            if (!active)
                return;

            // Update all systems
            for (auto& pair : systems)
            {
                pair.second->Update(deltaTime);
            }

            // Update all entities
            for (auto& entity : entities)
            {
                entity->OnUpdate(deltaTime);
            }
        }
    }//namespace ecs
}//namespace hgl
