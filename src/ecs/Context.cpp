#include<hgl/ecs/Context.h>
#include<algorithm>

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

            component_registry.clear();

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

        void ECSContext::Render(graph::RenderCmdBuffer *cmd, float deltaTime)
        {
            if (!active)
                return;

            for (auto& pair : render_systems)
            {
                pair.second->Update(deltaTime);
                pair.second->Render(cmd, deltaTime);
            }
        }

        void ECSContext::RegisterComponentInstance(size_t type_hash, const std::shared_ptr<Component>& comp)
        {
            if(!comp)
                return;

            auto &vec = component_registry[type_hash];
            vec.push_back(comp);
        }

        void ECSContext::UnregisterComponentInstance(size_t type_hash, Component* comp_ptr)
        {
            auto it = component_registry.find(type_hash);
            if(it == component_registry.end())
                return;

            auto &vec = it->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(), [comp_ptr](const std::weak_ptr<Component>& w){
                auto sp = w.lock();
                return !sp || sp.get()==comp_ptr;
            }), vec.end());
        }
    }//namespace ecs
}//namespace hgl
