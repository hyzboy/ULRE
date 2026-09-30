#pragma once

#include<hgl/ecs/core/Object.h>
#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/ecs/support/ComponentTypeTable.h>
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

            /// 组件槽位位掩码（v2 §3 的 5 类）；**单一写者 = 组件挂载/卸载路径**（`ReplaceComponent` 置位、
            /// `RemoveComponent`/`DetachAllComponents` 清位）。它就是 stage B `EntityGPU::type[16]` 的前身。
            uint32_t component_mask = 0;

            // Use hash_code instead of string for faster lookups
            hgl::UnorderedMap<std::size_t, std::shared_ptr<Component>> components;
            ECSContext *context = nullptr;   ///< 所属的 ECSContext，不拥有

            void RegisterToContext(size_t type_hash, const std::shared_ptr<Component>& comp);
            void UnregisterFromContext(size_t type_hash, Component* comp_ptr);
            void ReplaceComponent(size_t type_hash, const std::shared_ptr<Component>& component, const std::type_index& component_type, ComponentType slot);
            void MarkSceneDirty() const;

        public:

            explicit Entity(const std::string& name = "Entity");
            ~Entity() override;

            /// Get entity ID
            // GetEntityID：消除对 Object::GetID（对象 ID）的隐藏——EntityID
            // 与 Object 的 ID 是两套体系，同名隐藏易混淆
            EntityID GetEntityID() const { return id; }

            /// 组件槽位位掩码（O(1) 策略判定的输入；见 v2 §9.2 P3）
            uint32_t GetComponentMask() const { return component_mask; }

            /// 是否挂了某槽位的组件（O(1)，不查组件表）
            bool HasComponentType(ComponentType type) const { return ComponentMaskHas(component_mask,type); }

            /// 调试/测试用：把掩码与"该实体实际挂载的组件"逐一对照，返回是否一致
            bool VerifyComponentMaskAgainstComponents() const;

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
                ReplaceComponent(typeid(T).hash_code(), component, std::type_index(typeid(T)), ComponentTypeOf<T>::value);
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
                component_mask = ComponentMaskRemove(component_mask,ComponentTypeOf<T>::value);   // 掩码：唯一移除路径
                MarkSceneDirty();
            }

        public:

            /// Get all components (for serialization)
            void GetAllComponents(std::vector<std::shared_ptr<Component>>& out) const;

            /// Detach all components from this entity
            void DetachAllComponents(bool notify_systems = true);
        };
    }//namespace ecs
}//namespace hgl


