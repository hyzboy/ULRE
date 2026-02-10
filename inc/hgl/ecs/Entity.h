#pragma once

#include<hgl/ecs/Object.h>
#include<hgl/ecs/Component.h>
#include<hgl/ecs/EntityHandle.h>
#include<memory>
#include <hgl/type/UnorderedMap.h>
#include<typeinfo>
#include<typeindex>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;

        /**
         * Entity - represents game objects with components
         * Entities are containers for components
         */
        class Entity : public Object
        {
        private:

            EntityID id;
            // Use hash_code instead of string for faster lookups
            hgl::UnorderedMap<std::size_t, std::shared_ptr<Component>> components;
            ECSContext *context = nullptr;   ///< 所属的 ECSContext，不拥有

            void RegisterToContext(size_t type_hash, const std::shared_ptr<Component>& comp);
            void UnregisterFromContext(size_t type_hash, Component* comp_ptr);
            void NotifyComponentAdded(const std::type_index& component_type);
            void NotifyComponentRemoved(const std::type_index& component_type);

        public:

            explicit Entity(const std::string& name = "Entity");
            ~Entity() override;

            /// Get entity ID
            EntityID GetID() const { return id; }
            
            /// Set entity ID (called by EntityManager)
            void SetID(EntityID entity_id) { id = entity_id; }

            void SetContext(ECSContext *ctx) { context = ctx; }

            ECSContext *GetContext() const { return context; }

        public:

            /// Add component to entity
            template<typename T, typename... Args>
            std::shared_ptr<T> AddComponent(Args&&... args)
            {
                auto component = std::make_shared<T>(std::forward<Args>(args)...);
                components.ChangeOrAdd(typeid(T).hash_code(), component);
                component->SetOwner(id, context);
                RegisterToContext(typeid(T).hash_code(), component);
                component->OnAttach();
                NotifyComponentAdded(std::type_index(typeid(T)));  // Notify systems
                return component;
            }

            /// Get component by type
            template<typename T>
            std::shared_ptr<T> GetComponent() const
            {
                auto *component = components.GetValuePointer(typeid(T).hash_code());
                if (component)
                    return std::static_pointer_cast<T>(*component);
                return nullptr;
            }

            /// Check if entity has component
            template<typename T>
            bool HasComponent() const
            {
                return components.ContainsKey(typeid(T).hash_code());
            }

            /// Check if entity has component by type_index
            bool HasComponentByType(const std::type_index& type) const
            {
                return components.ContainsKey(type.hash_code());
            }

            /// Remove component by type
            template<typename T>
            void RemoveComponent()
            {
                const size_t type_hash = typeid(T).hash_code();
                auto *component = components.GetValuePointer(type_hash);
                if (!component)
                    return;

                UnregisterFromContext(type_hash, component->get());
                (*component)->OnDetach();
                components.DeleteByKey(type_hash);
                NotifyComponentRemoved(std::type_index(typeid(T)));  // Notify systems
            }

        public:

            /// Update all components
            void OnUpdate(float deltaTime) override;

            /// Get component count
            size_t GetComponentCount() const { return static_cast<size_t>(components.GetCount()); }

            /// Get all components (for serialization)
            void GetAllComponents(std::vector<std::shared_ptr<Component>>& out) const;

            /// Attach a pre-constructed component instance (for deserialization)
            void AddComponentInstance(const std::shared_ptr<Component>& component);
        };
    }//namespace ecs
}//namespace hgl

