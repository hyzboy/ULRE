#pragma once

#include<hgl/ecs/Object.h>

namespace hgl
{
    namespace ecs
    {
        /**
         * Base class for all systems
         * Systems handle specific types of logic and processing
         */
        class System : public Object
        {
        protected:

            bool initialized = false;

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

            /// Get if system is initialized
            bool IsInitialized() const { return initialized; }

        protected:

            void SetInitialized(bool value) { initialized = value; }
        };
    }//namespace ecs
}//namespace hgl
