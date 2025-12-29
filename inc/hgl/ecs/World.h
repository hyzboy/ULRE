#pragma once

#include<hgl/ecs/Object.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/System.h>
#include<memory>
#include<vector>
#include<unordered_map>
#include<typeinfo>

namespace hgl
{
    namespace ecs
    {
        /**
         * World manages all entities and systems
         * Acts as the main container for the ECS simulation
         */
        class World : public Object
        {
        private:

            std::vector<std::shared_ptr<Entity>> entities;
            std::unordered_map<size_t, std::shared_ptr<System>> systems;
            bool active = false;

        public:

            World(const std::string& name = "World");
            ~World() override;

        public:

            /// Initialize the world
            void Initialize();

            /// Shut down the world
            void Shutdown();

            /// Update all systems and entities
            void Update(float deltaTime);

        public:

            /// Create a new entity in the world
            template<typename T = Entity, typename... Args>
            std::shared_ptr<T> CreateEntity(Args&&... args)
            {
                auto entity = std::make_shared<T>(std::forward<Args>(args)...);
                entities.push_back(entity);
                entity->OnCreate();
                return entity;
            }

            /// Register a system
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterSystem(Args&&... args)
            {
                auto system = std::make_shared<T>(std::forward<Args>(args)...);
                systems[typeid(T).hash_code()] = system;
                return system;
            }

            /// Get a system by type
            template<typename T>
            std::shared_ptr<T> GetSystem() const
            {
                auto it = systems.find(typeid(T).hash_code());
                if (it != systems.end())
                {
                    return std::static_pointer_cast<T>(it->second);
                }
                return nullptr;
            }

        public:

            /// Get entity count
            size_t GetEntityCount() const { return entities.size(); }

            /// Get all entities (for systems that need to iterate)
            const std::vector<std::shared_ptr<Entity>>& GetEntities() const { return entities; }

            /// Check if world is active
            bool IsActive() const { return active; }
        };
    }//namespace ecs
}//namespace hgl
