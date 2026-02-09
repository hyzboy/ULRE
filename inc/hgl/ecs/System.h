#pragma once

#include<hgl/ecs/Object.h>
#include<hgl/ecs/EntityQuery.h>
#include<vector>
#include<typeindex>
#include<memory>

namespace hgl { namespace graph { class RenderCmdBuffer; } }

namespace hgl
{
    namespace ecs
    {
        /**
         * System type identifiers for dependency management
         */
        enum class SystemType
        {
            Unknown,
            Input,
            Transform,
            Camera,
            BoundingBox,
            RenderCollect,
            RenderBatch,
            RenderSubmit,
            Physics,
            Animation,
            // Add more as needed
        };

        /**
         * Base class for all systems
         * Systems handle specific types of logic and processing
         */
        class System : public Object
        {
        protected:

            bool initialized = false;
            SystemType systemType = SystemType::Unknown;
            int executionOrder = 0; // Lower values execute first
            std::vector<std::type_index> dependencies; // Type IDs of systems this depends on
            std::unique_ptr<SystemCache> cache_manager;  // Component query cache
            class ECSContext* context = nullptr;  // Owning context

        public:

            explicit System(const std::string& name = "System");
            virtual ~System() = default;

        public:

            /// Initialize the system
            virtual void Initialize() {}

            /// Shut down the system
            virtual void Shutdown() {}

            /// Update the system (called once per frame)
            virtual void Update(float deltaTime) {}

            /// Render hook (optional). Default no-op.
            virtual void Render(graph::RenderCmdBuffer *, float /*deltaTime*/ ) {}

            /// Get if system is initialized
            bool IsInitialized() const { return initialized; }

            /// Get system type
            SystemType GetSystemType() const { return systemType; }

            /// Get execution order (lower runs earlier)
            int GetExecutionOrder() const { return executionOrder; }

            /// Get dependencies (systems that must run before this one)
            const std::vector<std::type_index>& GetDependencies() const { return dependencies; }

            /// Called after all dependencies are ready
            virtual void OnDependenciesReady() {}

            /// Get the cache manager for this system
            SystemCache* GetCache();
            const SystemCache* GetCache() const;

            /// Manual participation: Explicitly add an entity to a query
            /// Used for custom logic where entity doesn't match standard signature
            /// Example: Add entity to LOD system when close to camera
            void AddEntityManually(EntityQuery* query, EntityID entity_id);

            /// Manual participation: Explicitly remove an entity from a query
            /// Used to remove entities from optional processing
            /// Example: Remove entity from AI update when too far from player
            void RemoveEntityManually(EntityQuery* query, EntityID entity_id);

            /// Set the context (called by Context when system is registered)
            void SetContext(ECSContext* ctx) { context = ctx; }

        protected:

            void SetInitialized(bool value) { initialized = value; }

            /// Set system type (call in derived constructor)
            void SetSystemType(SystemType type) { systemType = type; }

            /// Set execution order (call in derived constructor, lower runs first)
            void SetExecutionOrder(int order) { executionOrder = order; }

            /// Add a dependency to another system type
            template<typename T>
            void AddDependency()
            {
                dependencies.push_back(std::type_index(typeid(T)));
            }

            /// Create a query for finding entities with specific components
            /// Usage: auto query = CreateQuery<TransformComponent, RenderComponent>();
            template<typename FirstComponent, typename... RestComponents>
            EntityQuery* CreateQuery()
            {
                auto cache = GetCache();
                if (!cache)
                    return nullptr;
                return cache->CreateQuery<FirstComponent, RestComponents...>();
            }
        };
    }//namespace ecs
}//namespace hgl
