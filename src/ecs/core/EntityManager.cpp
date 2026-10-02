#include<hgl/ecs/core/EntityManager.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<algorithm>

namespace hgl::ecs
{
    EntityManager::EntityManager(uint32_t capacity)
        : max_entities(capacity)
    {
        slots.reserve(capacity);
    }

    EntityManager::~EntityManager()
    {
        Clear();
    }

    Entity *EntityManager::BlockEntityAt(uint32_t index)
    {
        const uint32_t block_index = index / kEntityBlockSize;

        while (blocks.size() <= block_index)
        {
            blocks.push_back(std::make_unique<std::byte[]>(sizeof(Entity) * kEntityBlockSize));
        }

        return reinterpret_cast<Entity *>(blocks[block_index].get()) + (index % kEntityBlockSize);
    }

    void EntityManager::DestroyEntity(EntityID id)
    {
        if (!IsValidID(id))
        {
            #ifdef _DEBUG
            LogWarning("[EntityManager] Attempting to destroy invalid entity ID");
            #endif
            return;
        }

        EntitySlot& slot = slots[id.index];
    #if ULRE_ECS_DEBUG_API
        LogDebug("[EntityManager] DestroyEntity id=(%u,%u) has_entity=%d",
             id.index,
             id.generation,
             slot.entity ? 1 : 0);
    #endif
        if (slot.entity)
        {
            slot.entity->DetachAllComponents(true);
            slot.entity->~Entity();      // 块池：显式析构；存储留待该槽复用（行/地址语义不变）
            slot.entity = nullptr;
        }
        slot.alive = false;
        slot.generation++;

        AddFreeIndex(id.index);
    }

    Entity* EntityManager::GetEntity(EntityID id)
    {
        if (!IsValidID(id))
            return nullptr;

        return slots[id.index].entity;
    }

    const Entity* EntityManager::GetEntity(EntityID id) const
    {
        if (!IsValidID(id))
            return nullptr;

        return slots[id.index].entity;
    }

    bool EntityManager::IsValidID(EntityID id) const
    {
        if (!id.IsValid() || id.index >= slots.size())
            return false;

        const EntitySlot& slot = slots[id.index];
        return slot.alive && slot.generation == id.generation;
    }

    void EntityManager::GetAllEntityPointers(std::vector<Entity*>& out_entities)
    {
        out_entities.clear();

        for (auto& slot : slots)
        {
            if (slot.alive && slot.entity)
            {
                out_entities.push_back(slot.entity);
            }
        }
    }

    void EntityManager::GetAllEntityPointers(std::vector<const Entity*>& out_entities) const
    {
        out_entities.clear();

        for (const auto& slot : slots)
        {
            if (slot.alive && slot.entity)
            {
                out_entities.push_back(slot.entity);
            }
        }
    }

    void EntityManager::Clear()
    {
        size_t detach_count = 0;

    #if ULRE_ECS_DEBUG_API
        LogDebug("[EntityManager] Clear begin slots=%zu", slots.size());
    #endif

        for (auto& slot : slots)
        {
            if (slot.entity)
            {
                slot.entity->DetachAllComponents(true);
                slot.entity->~Entity();      // 块池：显式析构
                ++detach_count;
            }
            slot.entity = nullptr;
            slot.alive = false;
        }
        slots.clear();
        free_indices.clear();
        blocks.clear();          ///< 块池一并释放（下次 CreateEntity 重新按需建块）
        next_index = 0;

    #if ULRE_ECS_DEBUG_API
        LogDebug("[EntityManager] Clear end detached=%zu", detach_count);
    #endif
    }

    void EntityManager::ExpandSlots(uint32_t new_capacity)
    {
        if (new_capacity <= max_entities)
            return;

        max_entities = new_capacity;
        slots.reserve(new_capacity);
    }

    void EntityManager::AddFreeIndex(uint32_t index)
    {
        auto it = std::lower_bound(free_indices.begin(), free_indices.end(), index);
        if (it == free_indices.end() || *it != index)
        {
            free_indices.insert(it, index);
        }
    }
}

