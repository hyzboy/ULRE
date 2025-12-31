#pragma once

#include<hgl/ecs/Object.h>
#include<hgl/ecs/Component.h>
#include<memory>
#include<unordered_map>
#include<typeinfo>

namespace hgl
{
    namespace ecs
    {
        /**
         * Entity - represents game objects with components
         * Entities are containers for components
         */
        class Entity : public Object
        {
        private:

            // Use hash_code instead of string for faster lookups
            std::unordered_map<std::size_t, std::shared_ptr<Component>> components;

        public:

            explicit Entity(const std::string& name = "Entity");
            ~Entity() override;

        public:

            /// Add component to entity
            template<typename T, typename... Args>
            std::shared_ptr<T> AddComponent(Args&&... args)
            {
                auto component = std::make_shared<T>(std::forward<Args>(args)...);
                components[typeid(T).hash_code()] = component;
                // Note: owner is not set automatically to avoid circular reference issues
                // Users can call component->SetOwner() manually if needed
                component->OnAttach();
                return component;
            }

            /// Get component by type
            template<typename T>
            std::shared_ptr<T> GetComponent() const
            {
                auto it = components.find(typeid(T).hash_code());
                if (it != components.end())
                {
                    return std::static_pointer_cast<T>(it->second);
                }
                return nullptr;
            }

            /// Check if entity has component
            template<typename T>
            bool HasComponent() const
            {
                return components.find(typeid(T).hash_code()) != components.end();
            }

            /// Remove component by type
            template<typename T>
            void RemoveComponent()
            {
                auto it = components.find(typeid(T).hash_code());
                if (it != components.end())
                {
                    it->second->OnDetach();
                    components.erase(it);
                }
            }

        public:

            /// Update all components
            void OnUpdate(float deltaTime) override;

            /// Get component count
            size_t GetComponentCount() const { return components.size(); }
        };
    }//namespace ecs
}//namespace hgl
