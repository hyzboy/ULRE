#pragma once

#include<hgl/ecs/Object.h>
#include<vector>
#include<typeindex>

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
        };
    }//namespace ecs
}//namespace hgl
