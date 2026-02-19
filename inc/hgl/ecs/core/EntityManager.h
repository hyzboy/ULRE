#pragma once

#include<hgl/ecs/core/EntityHandle.h>
#include<vector>
#include<memory>
#include<string>

namespace hgl::ecs
{
    class Entity;

    /// Manages entity lifecycle using ID system
    /// Maintains a pool of entities with generation-based handle validation
    class EntityManager
    {
    private:
        struct EntitySlot
        {
            std::unique_ptr<Entity> entity;
            uint16_t generation = 0;
            bool alive = false;
        };

        std::vector<EntitySlot> slots;
        std::vector<uint32_t> free_indices;
        uint32_t next_index = 0;
        uint32_t max_entities = 0;

    public:
        EntityManager(uint32_t capacity = 1000);
        ~EntityManager();

        /// Create new entity with given name
        EntityID CreateEntity(const std::string& name = "Entity");

        /// Create new entity from a pre-constructed instance
        EntityID CreateEntity(std::unique_ptr<Entity> entity);

        /// Destroy entity by ID
        void DestroyEntity(EntityID id);

        /// Get entity pointer by ID, returns nullptr if invalid
        Entity* GetEntity(EntityID id);
        const Entity* GetEntity(EntityID id) const;

        /// Check if ID is valid and points to alive entity
        bool IsValidID(EntityID id) const;

        /// Get count of alive entities
        uint32_t GetEntityCount() const;

        /// Get all alive entity IDs
        void GetAllEntities(std::vector<EntityID>& out_ids) const;

        /// Get all alive entity pointers
        void GetAllEntityPointers(std::vector<Entity*>& out_entities);
        void GetAllEntityPointers(std::vector<Entity*>& out_entities) const;

        /// Clear all entities
        void Clear();

        /// Get slot count (including dead slots)
        uint32_t GetSlotCount() const { return (uint32_t)slots.size(); }

        /// Get capacity
        uint32_t GetCapacity() const { return max_entities; }

    private:
        void ExpandSlots(uint32_t new_capacity);
        void AddFreeIndex(uint32_t index);
    };
}

