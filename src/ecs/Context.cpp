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
            for (auto& pair : tick_systems)
                pair.second->Initialize();

            for (auto& pair : render_systems)
                pair.second->Initialize();

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
            for (auto& pair : tick_systems)
                pair.second->Shutdown();
            tick_systems.clear();

            for (auto& pair : render_systems)
                pair.second->Shutdown();
            render_systems.clear();

            active = false;
            OnDestroy();
        }

        void ECSContext::Tick(float deltaTime)
        {
            if (!active)
                return;

            // Update non-render systems
            for (auto& pair : tick_systems)
                pair.second->Update(deltaTime);

            // Update all entities
            for (auto& entity : entities)
                entity->OnUpdate(deltaTime);
        }

        void ECSContext::Render(float deltaTime)
        {
            if (!active)
                return;

            // Update/render render-systems
            for (auto& pair : render_systems)
                pair.second->Update(deltaTime);
        }
    }//namespace ecs
}//namespace hgl
