#pragma once

#include<hgl/ecs/core/Object.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/System.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/core/EntityManager.h>
#include<hgl/ecs/core/SystemProfiler.h>
#include<memory>
#include<vector>
#include <hgl/type/UnorderedMap.h>
#include<typeinfo>
#include<map>
#include<hgl/ecs/core/MaterialPipelineKey.h>

namespace hgl { namespace graph { class RenderCmdBuffer; class CameraInfo; } }

namespace hgl
{
    namespace ecs
    {
        class MaterialBatch;
        class PrimitiveRenderItem;

        struct RenderFrameCache
        {
            std::vector<std::unique_ptr<PrimitiveRenderItem>> renderItems;
            std::map<MaterialPipelineKey, std::unique_ptr<MaterialBatch>> materialBatches;
            const graph::CameraInfo* cameraInfo = nullptr;
            uint32_t renderableCount = 0;

            RenderFrameCache() = default;
            ~RenderFrameCache();

            void BeginFrame();
        };

        /**
         * ECSContext manages all entities and systems
         * Acts as the main container for the ECS simulation
         */
        class ECSContext : public Object
        {
        private:

            std::unique_ptr<EntityManager> entity_manager;

            // 分类存储：更新系统与渲染系统分开
            hgl::UnorderedMap<size_t, std::shared_ptr<System>> tick_systems;
            hgl::UnorderedMap<size_t, std::shared_ptr<System>> render_systems;

            struct OrderedSystem
            {
                size_t key = 0;
                int phase = 0;          // ExecutionPhase as int
                int priority = 0;       // ExecutionPriority as int (for ordering within phase)
                uint64_t insertion_order = 0;  // For stable sorting
                std::shared_ptr<System> system;
            };

            using DependencyMap = hgl::UnorderedMap<size_t, std::vector<size_t>>;

            std::vector<OrderedSystem> tick_system_order;
            std::vector<OrderedSystem> render_system_order;
            bool tick_order_dirty = false;
            bool render_order_dirty = false;
            uint64_t next_system_order = 1;

            DependencyMap tick_dependencies;
            DependencyMap render_dependencies;

            // 组件注册表：按类型hash存储弱引用，便于系统快速查询
            hgl::UnorderedMap<size_t, std::vector<std::weak_ptr<Component>>> component_registry;

            // TransformComponent 分离列表
            std::vector<std::weak_ptr<TransformComponent>> static_transforms;
            std::vector<std::weak_ptr<TransformComponent>> movable_transforms;

            bool active = false;

            RenderFrameCache render_frame_cache;
            SystemProfiler profiler;
            bool system_profiling_enabled = true;
            uint32_t frame_index = 0;

            // SubWorld support - hierarchical context
            ECSContext* parent_context = nullptr;       // nullptr if this is a root context
            bool owns_transform_storage = false;         // true if this context created the storage

        private:

            void SortTickSystems();
            void SortRenderSystems();
            void SortSystemList(std::vector<OrderedSystem>& order_list,
                                const DependencyMap& dependencies,
                                bool& dirty_flag,
                                const char* label);
            OrderedSystem* FindOrderedSystem(std::vector<OrderedSystem>& list, size_t key);
            void AddOrUpdateSystem(bool is_render, size_t key, const std::shared_ptr<System>& system, int priority);
            void AddSystemDependency(bool is_render, size_t dependent_key, size_t dependency_key);
            void RunRenderPhaseUpdates(ExecutionPhase phase, float deltaTime);
            void RunRenderUpdatesFrom(ExecutionPhase phase, float deltaTime);
            void RunSystemUpdate(System *system, float deltaTime);

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

            /// Run pre-begin-frame render updates (no command buffer)
            void RenderPreBeginFrame(float deltaTime);

            /// Run begin-frame render updates (frame index available)
            void RenderBeginFrame(float deltaTime);

            /// Run post-begin-frame render updates (no command buffer)
            void RenderPostBeginFrame(float deltaTime);

            /// Clear all entities and component registries
            void ClearEntities();

            /// Serialize world to JSON
            bool SaveToJson(const std::string& path) const;

            /// Deserialize world from JSON (IDs remapped)
            bool LoadFromJson(const std::string& path);

            /// Serialize world to binary
            bool SaveToBinary(const std::string& path) const;

            /// Deserialize world from binary (IDs remapped)
            bool LoadFromBinary(const std::string& path);

            void SetSystemProfilingEnabled(bool enabled) { system_profiling_enabled = enabled; }
            bool IsSystemProfilingEnabled() const { return system_profiling_enabled; }
            SystemProfiler& GetSystemProfiler() { return profiler; }
            const SystemProfiler& GetSystemProfiler() const { return profiler; }

            void SetFrameIndex(const uint32_t index);
            uint32_t GetFrameIndex() const { return frame_index; }

            /// 注册组件实例（由 Entity::AddComponent 调用）
            void RegisterComponentInstance(size_t type_hash, const std::shared_ptr<Component>& comp);

            /// 反注册组件实例（由 Entity::RemoveComponent 调用）
            void UnregisterComponentInstance(size_t type_hash, Component* comp_ptr);

            /// Notify all systems that an entity gained a component (for cache invalidation)
            void NotifyComponentAdded(EntityID entity_id, const std::type_index& component_type);

            /// Notify all systems that an entity lost a component (for cache invalidation)
            void NotifyComponentRemoved(EntityID entity_id, const std::type_index& component_type);

            /// Register a transform component (called by TransformComponent::OnAttach)
            void RegisterTransformComponent(const std::shared_ptr<TransformComponent>& comp, bool isMovable);

            /// Migrate transform between static and movable lists
            void MigrateTransformComponent(TransformComponent* comp_ptr, bool toMovable);

            /// Unregister a transform component (called by TransformComponent::OnDetach)
            void UnregisterTransformComponent(TransformComponent* comp_ptr);

            /// Get static transforms for offline baking
            const std::vector<std::weak_ptr<TransformComponent>>& GetStaticTransforms() const { return static_transforms; }

            /// Get movable transforms for runtime updates
            const std::vector<std::weak_ptr<TransformComponent>>& GetMovableTransforms() const { return movable_transforms; }

        public:

            /// Create a new entity in the world
            /// Returns raw pointer managed by EntityManager
            template<typename T = Entity, typename... Args>
            T* CreateEntity(Args&&... args)
            {
                EntityID id = entity_manager->CreateEntity();
                Entity* entity = entity_manager->GetEntity(id);
                
                if constexpr (std::is_same_v<T, Entity>)
                {
                    entity->SetContext(this);
                    entity->OnCreate();
                    return (T*)entity;
                }
                else
                {
                    // For derived types, this won't work correctly
                    // Derived entity types should be created with EntityManager directly
                    ((T*)entity)->SetContext(this);
                    ((T*)entity)->OnCreate();
                    return (T*)entity;
                }
            }

            /// Get entity by ID
            Entity* GetEntity(EntityID id)
            {
                if (!entity_manager)
                    return nullptr;
                return entity_manager->GetEntity(id);
            }

            /// Get entity by ID (const version)
            const Entity* GetEntity(EntityID id) const
            {
                if (!entity_manager)
                    return nullptr;
                return entity_manager->GetEntity(id);
            }

            /// Destroy entity by ID
            void DestroyEntity(EntityID id)
            {
                if (entity_manager)
                    entity_manager->DestroyEntity(id);
            }

            /// Get all alive entity IDs
            void GetAllEntityIDs(std::vector<EntityID>& out_ids) const
            {
                if (entity_manager)
                    entity_manager->GetAllEntities(out_ids);
            }

            /// Get all alive entity pointers
            void GetAllEntities(std::vector<Entity*>& out_entities)
            {
                if (entity_manager)
                    entity_manager->GetAllEntityPointers(out_entities);
            }

            /// Get all alive entity pointers (const version)
            void GetAllEntities(std::vector<Entity*>& out_entities) const
            {
                if (entity_manager)
                    entity_manager->GetAllEntityPointers(out_entities);
            }

            /// Register a system
            /// @param is_render true则放入渲染系统列表，否则为逻辑更新系统
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterSystem(bool is_render, Args&&... args)
            {
                auto system = std::make_shared<T>(std::forward<Args>(args)...);
                const size_t key = typeid(T).hash_code();
                AddOrUpdateSystem(is_render, key, system, 0);
                return system;
            }

            /// Register a system with explicit priority (lower runs earlier)
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterSystemWithPriority(bool is_render, int priority, Args&&... args)
            {
                auto system = std::make_shared<T>(std::forward<Args>(args)...);
                const size_t key = typeid(T).hash_code();
                AddOrUpdateSystem(is_render, key, system, priority);
                return system;
            }

            /// Register a tick (logic) system
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterTickSystem(Args&&... args)
            {
                return RegisterSystem<T>(false, std::forward<Args>(args)...);
            }

            /// Register a tick (logic) system with explicit priority
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterTickSystemWithPriority(int priority, Args&&... args)
            {
                return RegisterSystemWithPriority<T>(false, priority, std::forward<Args>(args)...);
            }

            /// Register a render system
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterRenderSystem(Args&&... args)
            {
                return RegisterSystem<T>(true, std::forward<Args>(args)...);
            }

            /// Register a render system with explicit priority
            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterRenderSystemWithPriority(int priority, Args&&... args)
            {
                return RegisterSystemWithPriority<T>(true, priority, std::forward<Args>(args)...);
            }

            /// Update system priority (lower runs earlier)
            template<typename T>
            bool SetSystemPriority(int priority)
            {
                const size_t key = typeid(T).hash_code();

                if (tick_systems.ContainsKey(key))
                {
                    if (auto *entry = FindOrderedSystem(tick_system_order, key))
                    {
                        entry->priority = priority;
                        tick_order_dirty = true;
                        return true;
                    }
                }

                if (render_systems.ContainsKey(key))
                {
                    if (auto *entry = FindOrderedSystem(render_system_order, key))
                    {
                        entry->priority = priority;
                        render_order_dirty = true;
                        return true;
                    }
                }

                return false;
            }

             /// Get a system by type
             template<typename T>
             std::shared_ptr<T> GetSystem() const
             {
                const size_t key = typeid(T).hash_code();

                if (auto *system = tick_systems.GetValuePointer(key))
                    return std::static_pointer_cast<T>(*system);

                if (auto *system = render_systems.GetValuePointer(key))
                    return std::static_pointer_cast<T>(*system);
                 return nullptr;
             }

        public:
            /// Get entity count
            size_t GetEntityCount() const 
            { 
                return entity_manager ? entity_manager->GetEntityCount() : 0;
            }

            RenderFrameCache& GetRenderFrameCache() { return render_frame_cache; }
            const RenderFrameCache& GetRenderFrameCache() const { return render_frame_cache; }

            /// Check if world is active
            bool IsActive() const { return active; }

            /// SubWorld Hierarchical Support ///
            
            /// Attach this context as a child to a parent context (for SubWorld support)
            /// Shares the parent's TransformDataStorage for seamless parent-child relationships
            void AttachToParent(ECSContext* parent);

            /// Get parent context (nullptr if root)
            ECSContext* GetParentContext() const { return parent_context; }

            /// Check if this context owns its transform storage
            bool OwnsTransformStorage() const { return owns_transform_storage; }

            /// 获取指定类型的组件列表（自动清理已失效的弱引用）
            template<typename T>
            void GetComponents(std::vector<std::shared_ptr<T>>& out) const
            {
                out.clear();
                const size_t key = typeid(T).hash_code();
                auto *list = component_registry.GetValuePointer(key);
                if (!list)
                    return;

                for(auto &weak_comp : *list)
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


