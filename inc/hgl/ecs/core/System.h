#pragma once

#include<hgl/ecs/core/Object.h>
#include<hgl/ecs/core/EntityQuery.h>
#include<hgl/log/Log.h>
#include<vector>
#include<typeindex>
#include<memory>

namespace hgl { namespace graph { class RenderCmdBuffer; } }

namespace hgl
{
    namespace ecs
    {
        /**
         * Execution Phase - Defines the stage (may have multiple systems per phase)
         * ordered by enum value; systems with same phase use Priority to order
         */
        enum class ExecutionPhase
        {
            // ===== Tick Phase =====
            TickInput,           // Keyboard/mouse
            TickTransform,       // Transform calculations
            TickCamera,          // Camera setup

            // ===== Pre-Render Phase =====
            RenderSwapchainNextImage, // Acquire swapchain image (no command buffer)
            RenderPreBeginFrame,      // Pre-BeginFrame updates (no render target frame index)
            RenderBeginFrame,         // BeginFrame updates (frame index available)
            RenderPostBeginFrame,     // Post-BeginFrame updates (frame index available)

            // ===== Render Collection Phase (may have multiple collectors) =====
            RenderCollect,       // Collect render data - can have multiple

            // ===== Render Batch Phase =====
            RenderBatch,         // Batch render data

            // ===== Render Submit Phase =====
            RenderDrawSubmit,    // Submit draw calls - can have multiple

            // ===== Post-Render Phase =====
            RenderPostProcess,   // Line rendering, post-effects, etc

            // ===== Frame Submit Phase =====
            RenderSubmit         // Submit frame to swapchain/present
        };

        /**
         * Execution Priority - Determines order WITHIN the same phase
         * Lower values run first
         */
        enum class ExecutionPriority
        {
            First = 0,
            Second = 10,
            Third = 20,
            Fourth = 30,
            Fifth = 40,
            Last = 100
        };

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
            Material,
            // Add more as needed
        };

        /**
         * Base class for all systems
         * Systems handle specific types of logic and processing
         */
        class System : public Object
        {
        protected:

            OBJECT_LOGGER

            bool initialized = false;
            SystemType systemType = SystemType::Unknown;
            ExecutionPhase executionPhase = ExecutionPhase::TickInput;
            ExecutionPriority executionPriority = ExecutionPriority::First;
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

            /// Get execution phase
            ExecutionPhase GetExecutionPhase() const { return executionPhase; }

            /// Get execution priority (within phase)
            ExecutionPriority GetExecutionPriority() const { return executionPriority; }

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

            /// Set execution order by phase and priority within phase
            void SetExecutionOrder(ExecutionPhase phase, ExecutionPriority priority = ExecutionPriority::First)
            {
                executionPhase = phase;
                executionPriority = priority;
            }

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

