#include<hgl/ecs/Context.h>
#include<hgl/ecs/TransformSystem.h>
#include"ECSTransformAssignmentBuffer.h"
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
            // Ensure TransformSystem is registered and bound to this world
            {
                auto transform_system = GetSystem<TransformSystem>();
                if (!transform_system)
                {
                    transform_system = RegisterTickSystem<TransformSystem>();
                }

                if (transform_system)
                {
                    transform_system->SetWorld(this);
                }
            }

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
            static_transforms.clear();
            movable_transforms.clear();

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
            }

            if (auto transform_system = GetSystem<TransformSystem>())
            {
                transform_system->SubmitTransformUpdates();
            }

            for (auto& pair : render_systems)
            {
                pair.second->Render(cmd, deltaTime);
            }
        }

        void ECSContext::SetFrameIndex(const uint32_t index)
        {
            ECSTransformAssignmentBuffer::SetFrameIndex(index);
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

        void ECSContext::RegisterTransformComponent(const std::shared_ptr<TransformComponent>& comp, bool isMovable)
        {
            if (!comp)
                return;

            if (isMovable)
            {
                movable_transforms.push_back(comp);
            }
            else
            {
                static_transforms.push_back(comp);
            }
        }

        void ECSContext::MigrateTransformComponent(TransformComponent* comp_ptr, bool toMovable)
        {
            if (!comp_ptr)
                return;

            auto remove_from_list = [comp_ptr](std::vector<std::weak_ptr<TransformComponent>>& list)
            {
                list.erase(std::remove_if(list.begin(), list.end(),
                    [comp_ptr](const std::weak_ptr<TransformComponent>& w)
                    {
                        auto sp = w.lock();
                        return !sp || sp.get() == comp_ptr;
                    }), list.end());
            };

            // Remove from current list
            if (toMovable)
            {
                remove_from_list(static_transforms);
            }
            else
            {
                remove_from_list(movable_transforms);
            }

            // Add to new list (need shared_ptr, so use owner if possible)
            // For now, we can't directly add without shared_ptr, so caller must handle
            // This is called after MigrateStorage(), so the component is already in correct storage
        }

        void ECSContext::UnregisterTransformComponent(TransformComponent* comp_ptr)
        {
            if (!comp_ptr)
                return;

            auto remove_from_list = [comp_ptr](std::vector<std::weak_ptr<TransformComponent>>& list)
            {
                list.erase(std::remove_if(list.begin(), list.end(),
                    [comp_ptr](const std::weak_ptr<TransformComponent>& w)
                    {
                        auto sp = w.lock();
                        return !sp || sp.get() == comp_ptr;
                    }), list.end());
            };

            remove_from_list(static_transforms);
            remove_from_list(movable_transforms);
        }
    }//namespace ecs
}//namespace hgl
