#include<hgl/ecs/Context.h>

namespace hgl
{
    namespace ecs
    {
        ECSContext::ECSContext(const std::string& name)
            : Object(name)
            , active(false)
        {
        }

        ECSContext::~ECSContext()
        {
            Shutdown();
        }

        void ECSContext::Initialize()
        {
            // Initialize all systems
            for (auto& pair : systems)
            {
                pair.second->Initialize();
            }

            active = true;
            OnCreate();
        }

        void ECSContext::Shutdown()
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

        void ECSContext::Update(float deltaTime)
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
