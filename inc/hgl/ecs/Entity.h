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
        class ECSContext;

        /**
         * Entity - represents game objects with components
         * Entities are containers for components
         */
        class Entity : public Object, public std::enable_shared_from_this<Entity>
        {
        private:

            // Use hash_code instead of string for faster lookups
            std::unordered_map<std::size_t, std::shared_ptr<Component>> components;
            ECSContext *context = nullptr;   ///< 所属的 ECSContext，不拥有

            void RegisterToContext(size_t type_hash, const std::shared_ptr<Component>& comp);
            void UnregisterFromContext(size_t type_hash, Component* comp_ptr);

        public:

            explicit Entity(const std::string& name = "Entity");
            ~Entity() override;

            void SetContext(ECSContext *ctx) { context = ctx; }

        public:

            /// Add component to entity
            template<typename T, typename... Args>
            std::shared_ptr<T> AddComponent(Args&&... args)
            {
                auto component = std::make_shared<T>(std::forward<Args>(args)...);
                components[typeid(T).hash_code()] = component;
                component->SetOwner(shared_from_this());
                RegisterToContext(typeid(T).hash_code(), component);
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
                    UnregisterFromContext(typeid(T).hash_code(), it->second.get());
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
