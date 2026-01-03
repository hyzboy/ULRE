#pragma once

#include<hgl/ecs/Object.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/System.h>
#include<hgl/ecs/Component.h>
#include<memory>
#include<vector>
#include<unordered_map>
#include<typeinfo>

namespace hgl { namespace graph { class RenderCmdBuffer; } }

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

            // 组件注册表：按类型hash存储弱引用，便于系统快速查询
            std::unordered_map<size_t, std::vector<std::weak_ptr<Component>>> component_registry;

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
            void Render(graph::RenderCmdBuffer *cmd, float deltaTime);

            /// 注册组件实例（由 Entity::AddComponent 调用）
            void RegisterComponentInstance(size_t type_hash, const std::shared_ptr<Component>& comp);

            /// 反注册组件实例（由 Entity::RemoveComponent 调用）
            void UnregisterComponentInstance(size_t type_hash, Component* comp_ptr);

        public:

            /// Create a new entity in the world
            template<typename T = Entity, typename... Args>
            std::shared_ptr<T> CreateEntity(Args&&... args)
            {
                auto entity = std::make_shared<T>(std::forward<Args>(args)...);
                entity->SetContext(this);
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

            /// 获取指定类型的组件列表（自动清理已失效的弱引用）
            template<typename T>
            void GetComponents(std::vector<std::shared_ptr<T>>& out) const
            {
                out.clear();
                const size_t key = typeid(T).hash_code();
                auto it = component_registry.find(key);
                if(it == component_registry.end())
                    return;

                for(auto &weak_comp : it->second)
                {
                    if(auto comp = weak_comp.lock())
                    {
                        out.push_back(std::static_pointer_cast<T>(comp));
                    }
                }
            }
        };
    }//namespace ecs
}//namespace hgl
