#pragma once

#include<string>
#include<memory>
#include<hgl/ecs/EntityHandle.h>

namespace hgl
{
    namespace ecs
    {
        class Entity; // Forward declaration
        class ECSContext; // Forward declaration

        /**
         * Base component class for Entity
         * Components provide data and behavior to entities
         */
        class Component : public std::enable_shared_from_this<Component>
        {
        protected:

            std::string componentName;
            EntityID owner_id;
            ECSContext* owner_context = nullptr;

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

            /// Set the owner entity by ID
            void SetOwner(EntityID id, ECSContext* context = nullptr) 
            { 
                owner_id = id;
                owner_context = context;
            }

            /// Get the owner entity ID
            EntityID GetOwnerID() const { return owner_id; }

            /// Get the owner entity (returns nullptr if ID is invalid)
            Entity* GetOwner() const;
        };
    }//namespace ecs
}//namespace hgl
