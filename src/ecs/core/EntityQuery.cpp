#include<hgl/ecs/core/EntityQuery.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<algorithm>

namespace hgl::ecs
{
    size_t EntityQuery::Rebuild()
    {
        if (!context || required_components.empty())
        {
            dirty = false;
            return 0;
        }

        // Initial scan: find all entities matching this query
        std::vector<EntityID> new_entities;
        std::vector<EntityID> all_entity_ids;
        context->GetAllEntityIDs(all_entity_ids);

        for (const EntityID& entity_id : all_entity_ids)
        {
            Entity* entity = context->GetEntity(entity_id);
            if (!entity)
                continue;

            if (Matches(entity_id))
            {
                new_entities.push_back(entity_id);
            }
        }

        size_t count = new_entities.size();
        cached_entities = std::move(new_entities);
        dirty = false;
        return count;
    }

    bool EntityQuery::MatchesSignature(EntityID id) const
    {
        if (!context || id.index >= 0xFFFFFFFF)
            return false;

        Entity* entity = context->GetEntity(id);
        if (!entity)
            return false;

        // Check component signature only (ignore predicate)
        for (const auto& component_type : required_components)
        {
            if (!entity->HasComponentByType(component_type))
                return false;
        }

        return true;
    }

    bool EntityQuery::Matches(EntityID id) const
    {
        if (!MatchesSignature(id))
            return false;

        // If has predicate, check it too
        if (predicate)
        {
            Entity* entity = context->GetEntity(id);
            if (!entity)
                return false;
            return predicate(entity);
        }

        return true;
    }

    bool EntityQuery::TryAddEntity(EntityID id, const Entity* entity)
    {
        // Check if entity already in cache
        auto it = std::find(cached_entities.begin(), cached_entities.end(), id);
        if (it != cached_entities.end())
            return false;  // Already in cache

        // Check signature
        if (!MatchesSignature(id))
            return false;

        // Check predicate if exists
        if (predicate && !predicate(entity))
            return false;

        // Add to cache
        cached_entities.push_back(id);
        return true;
    }

    bool EntityQuery::TryRemoveEntity(EntityID id)
    {
        auto it = std::find(cached_entities.begin(), cached_entities.end(), id);
        if (it == cached_entities.end())
            return false;  // Not in cache

        cached_entities.erase(it);
        return true;
    }

    size_t EntityQuery::RemoveInvalidEntities()
    {
        if (!context || cached_entities.empty())
            return 0;

        const size_t old_size = cached_entities.size();

        cached_entities.erase(
            std::remove_if(cached_entities.begin(), cached_entities.end(),
                           [this](const EntityID& id)
                           {
                               return context->GetEntity(id) == nullptr;
                           }),
            cached_entities.end());

        return old_size - cached_entities.size();
    }

    void SystemCache::OnComponentAdded(EntityID entity_id, const std::type_index& component_type, const Entity* entity)
    {
        // Reactive mode: try to add entity to queries that need this component
        for (auto& query : queries)
        {
            const auto& required = query->GetRequiredComponents();
            auto it = std::find(required.begin(), required.end(), component_type);
            if (it != required.end())
            {
                // This query needs this component, try to add the entity
                query->TryAddEntity(entity_id, entity);
            }
        }
    }

    void SystemCache::OnComponentRemoved(EntityID entity_id, const std::type_index& component_type)
    {
        // Reactive mode: remove entity from queries that required this component
        for (auto& query : queries)
        {
            const auto& required = query->GetRequiredComponents();
            auto it = std::find(required.begin(), required.end(), component_type);
            if (it != required.end())
            {
                // This query required this component, remove the entity
                query->TryRemoveEntity(entity_id);
            }
        }
    }

    void SystemCache::OnEntityDestroyed(EntityID entity_id)
    {
        for (auto& query : queries)
        {
            query->TryRemoveEntity(entity_id);
        }
    }

    void SystemCache::AddEntityManually(EntityQuery* query, EntityID entity_id, const Entity* entity)
    {
        if (!query)
            return;

        // Manual mode: directly add to specified query
        query->TryAddEntity(entity_id, entity);
    }

    void SystemCache::RemoveEntityManually(EntityQuery* query, EntityID entity_id)
    {
        if (!query)
            return;

        // Manual mode: directly remove from specified query
        query->TryRemoveEntity(entity_id);
    }

    size_t SystemCache::RemoveInvalidEntities()
    {
        size_t removed = 0;
        for (auto& query : queries)
        {
            removed += query->RemoveInvalidEntities();
        }
        return removed;
    }
}

