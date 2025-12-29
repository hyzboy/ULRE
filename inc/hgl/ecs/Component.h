#pragma once

#include<string>
#include<memory>

namespace hgl
{
    namespace ecs
    {
        class Entity; // Forward declaration

        /**
         * Base component class for Entity
         * Components provide data and behavior to entities
         */
        class Component
        {
        protected:

            std::string componentName;
            std::weak_ptr<Entity> owner;

        public:

            explicit Component(const std::string& name = "Component");
            virtual ~Component() = default;

        public:

            /// Called when component is attached to an entity
            virtual void OnAttach() {}

            /// Called each frame
            virtual void OnUpdate(float deltaTime) {}

            /// Called when component is detached from an entity
            virtual void OnDetach() {}

        public:

            const std::string& GetName() const { return componentName; }

            /// Set the owner entity
            void SetOwner(std::shared_ptr<Entity> entity) { owner = entity; }

            /// Get the owner entity
            std::shared_ptr<Entity> GetOwner() const { return owner.lock(); }
        };
    }//namespace ecs
}//namespace hgl
