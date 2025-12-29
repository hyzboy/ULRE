#pragma once

#include<cstdint>
#include<string>
#include<memory>

namespace hgl
{
    namespace ecs
    {
        /**
         * Base class for all ECS objects
         * Provides fundamental object lifetime and identification
         */
        class Object
        {
        public:

            using ObjectID = uint64_t;
            static constexpr ObjectID INVALID_OBJECT_ID = 0;

        protected:

            ObjectID objectId;
            std::string objectName;

        private:

            static ObjectID nextObjectId;

        public:

            explicit Object(const std::string& name = "Object");
            virtual ~Object() = default;

            // Delete copy operations
            Object(const Object&) = delete;
            Object& operator=(const Object&) = delete;

            // Allow move operations
            Object(Object&&) noexcept = default;
            Object& operator=(Object&&) noexcept = default;

        public:

            /// Get unique object ID
            ObjectID GetID() const { return objectId; }

            /// Get object name
            const std::string& GetName() const { return objectName; }

            /// Set object name
            void SetName(const std::string& name) { objectName = name; }

            /// Check if object is valid
            bool IsValid() const { return objectId != INVALID_OBJECT_ID; }

        public: // Lifecycle methods

            /// Called when object is created
            virtual void OnCreate() {}

            /// Called every frame for updates
            virtual void OnUpdate(float deltaTime) {}

            /// Called when object is destroyed
            virtual void OnDestroy() {}
        };
    }//namespace ecs
}//namespace hgl
