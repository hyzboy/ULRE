#include<hgl/ecs/core/EntityManager.h>
#include<hgl/ecs/core/Entity.h>
#include<algorithm>
#include<iostream>

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

    EntityID EntityManager::CreateEntity(const std::string& name)
    {
        return CreateEntity(std::make_unique<Entity>(name));
    }

    EntityID EntityManager::CreateEntity(std::unique_ptr<Entity> entity)
    {
        if (!entity)
            entity = std::make_unique<Entity>("Entity");

        uint32_t index;
        uint16_t generation = 0;

        if (!free_indices.empty())
        {
            index = free_indices.back();
            free_indices.pop_back();
            generation = slots[index].generation;
        }
        else
        {
            index = next_index;

            if (index >= slots.size())
            {
                if (index >= max_entities)
                {
                    ExpandSlots(max_entities * 2);
                }
                slots.resize(index + 1);
            }

            next_index++;
        }

        EntityID id(index, generation);
        EntitySlot& slot = slots[index];
        slot.entity = std::move(entity);
        slot.entity->SetID(id);
        slot.alive = true;
        slot.generation = generation;

        return id;
    }

    void EntityManager::DestroyEntity(EntityID id)
    {
        if (!IsValidID(id))
        {
            #ifdef _DEBUG
            std::cerr << "[EntityManager] WARNING: Attempting to destroy invalid entity ID" << std::endl;
            #endif
            return;
        }

        EntitySlot& slot = slots[id.index];
        if (slot.entity)
        {
            slot.entity->DetachAllComponents(true);
            slot.entity->OnDestroy();
        }
        slot.entity.reset();
        slot.alive = false;
        slot.generation++;

        AddFreeIndex(id.index);
    }

    Entity* EntityManager::GetEntity(EntityID id)
    {
        if (!IsValidID(id))
            return nullptr;

        return slots[id.index].entity.get();
    }

    const Entity* EntityManager::GetEntity(EntityID id) const
    {
        if (!IsValidID(id))
            return nullptr;

        return slots[id.index].entity.get();
    }

    bool EntityManager::IsValidID(EntityID id) const
    {
        if (!id.IsValid() || id.index >= slots.size())
            return false;

        const EntitySlot& slot = slots[id.index];
        return slot.alive && slot.generation == id.generation;
    }

    uint32_t EntityManager::GetEntityCount() const
    {
        uint32_t count = 0;
        for (const auto& slot : slots)
        {
            if (slot.alive)
                count++;
        }
        return count;
    }

    void EntityManager::GetAllEntities(std::vector<EntityID>& out_ids) const
    {
        out_ids.clear();

        for (uint32_t i = 0; i < slots.size(); ++i)
        {
            if (slots[i].alive)
            {
                out_ids.push_back(EntityID(i, slots[i].generation));
            }
        }
    }

    void EntityManager::GetAllEntityPointers(std::vector<Entity*>& out_entities)
    {
        out_entities.clear();

        for (auto& slot : slots)
        {
            if (slot.alive && slot.entity)
            {
                out_entities.push_back(slot.entity.get());
            }
        }
    }

    void EntityManager::GetAllEntityPointers(std::vector<Entity*>& out_entities) const
    {
        out_entities.clear();

        for (const auto& slot : slots)
        {
            if (slot.alive && slot.entity)
            {
                out_entities.push_back(slot.entity.get());
            }
        }
    }

    void EntityManager::Clear()
    {
        for (auto& slot : slots)
        {
            if (slot.entity)
            {
                slot.entity->DetachAllComponents(true);
                slot.entity->OnDestroy();
            }
            slot.entity.reset();
            slot.alive = false;
        }
        slots.clear();
        free_indices.clear();
        next_index = 0;
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

