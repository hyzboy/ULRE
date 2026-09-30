#pragma once

#include<string>
#include<memory>
#include<cstdint>
#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/ecs/support/ComponentTypeTable.h>

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
            friend class Entity;     ///< 只有 Entity 的挂载/卸载路径可以写槽位（单一写者）

        protected:

            std::string componentName;
            EntityID owner_id;
            ECSContext* owner_context = nullptr;
            Entity* owner_entity = nullptr;
            uint64_t version = 0;
            uint32_t change_mask = 0;

        private:

            /// 该组件占用的 Entity 槽位（无槽位 = None）。由 `Entity::ReplaceComponent` 写入（单一写者）。
            ComponentType component_slot = ComponentType::None;

            /// 槽位写入（私有：只有 friend Entity 的挂载路径能调）
            void SetComponentSlot(ComponentType slot) { component_slot = slot; }

        public:

            explicit Component(const std::string& name = "Component");
            virtual ~Component() = default;

        public:

            /// Called when component is attached to an entity
            virtual void OnAttach() {}

            /// Called when component is detached from an entity
            virtual void OnDetach() {}

            /// Return system group name this component belongs to.
            /// Default: nullptr (component does not require auto system-group activation)
            virtual const char *GetSystemGroupName() const { return nullptr; }

        public:

            const std::string& GetName() const { return componentName; }

            uint64_t GetVersion() const { return version; }

            uint32_t GetChangeMask() const { return change_mask; }

            /// 该组件占用的 Entity 槽位（无槽位 = None）。只读；写入由 Entity 挂载路径负责。
            ComponentType GetComponentSlot() const { return component_slot; }

            void ClearAllChanges() { change_mask = 0; }

            /// Set the owner entity by ID
            void SetOwner(EntityID id, ECSContext* context = nullptr)
            {
                owner_id = id;
                owner_context = context;
            }

            void SetOwnerEntity(Entity* entity)
            {
                owner_entity = entity;
            }

            /// Get the owner entity ID
            EntityID GetOwnerID() const { return owner_id; }

            /// Get the owner entity (returns nullptr if ID is invalid)
            Entity* GetOwner() const;

        protected:

            void TouchChange(uint32_t mask)
            {
                ++version;
                change_mask |= mask;
            }

            void AddChangeMask(uint32_t mask)
            {
                change_mask |= mask;
            }
        };
    }//namespace ecs
}//namespace hgl

