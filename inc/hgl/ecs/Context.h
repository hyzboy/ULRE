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
         * ECSContext manages all entities and systems
         * Acts as the main container for the ECS simulation
         */
        class ECSContext : public Object
        {
        private:

            std::vector<std::shared_ptr<Entity>> entities;

            // 分类存储：更新系统与渲染系统分开
            std::unordered_map<size_t, std::shared_ptr<System>> tick_systems;
            std::unordered_map<size_t, std::shared_ptr<System>> render_systems;

            bool active = false;

        public:

            ECSContext(const std::string& name = "World");
            ~ECSContext() override;

        public:

            /// Initialize the world
            void Initialize();

            /// Shut down the world
            void Shutdown();

            /// Tick all non-render systems and entities
            void Tick(float deltaTime);

            /// Run all render systems
            void Render(float deltaTime);

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
            /// @param is_render true则放入渲染系统列表，否则为逻辑更新系统
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterSystem(bool is_render, Args&&... args)
            {
                auto system = std::make_shared<T>(std::forward<Args>(args)...);
                const size_t key = typeid(T).hash_code();

                if(is_render)
                    render_systems[key] = system;
                else
                    tick_systems[key] = system;

                return system;
            }

            /// Register a tick (logic) system
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterTickSystem(Args&&... args)
            {
                return RegisterSystem<T>(false, std::forward<Args>(args)...);
            }

            /// Register a render system
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterRenderSystem(Args&&... args)
            {
                return RegisterSystem<T>(true, std::forward<Args>(args)...);
            }

             /// Get a system by type
             template<typename T>
             std::shared_ptr<T> GetSystem() const
             {
                const size_t key = typeid(T).hash_code();

                auto it = tick_systems.find(key);
                if (it != tick_systems.end())
                    return std::static_pointer_cast<T>(it->second);

                it = render_systems.find(key);
                if (it != render_systems.end())
                    return std::static_pointer_cast<T>(it->second);
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
