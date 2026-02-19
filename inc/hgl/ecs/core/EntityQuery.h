#pragma once

#include<hgl/ecs/core/EntityHandle.h>
#include<vector>
#include<memory>
#include<typeinfo>
#include<typeindex>
#include<functional>

namespace hgl::ecs
{
    class Entity;
    class ECSContext;

    /**
     * EntityQuery - Caches entities matching a specific component signature with optional filtering
     * Supports three participation modes:
     * 1. Mandatory: Must have all required components
     * 2. Conditional: Must have components AND pass predicate (e.g., distance, visibility)
     * 3. Manual: System explicitly adds/removes entities
     */
    class EntityQuery
    {
    public:
        using Predicate = std::function<bool(const Entity*)>;

    private:
        std::vector<std::type_index> required_components;
        std::vector<EntityID> cached_entities;
        ECSContext* context = nullptr;
        bool dirty = true;  // If true, cache needs rebuild
        Predicate predicate = nullptr;  // Optional filter function

    public:
        EntityQuery(ECSContext* ctx) : context(ctx) {}

        /// Add a required component type
        template<typename T>
        EntityQuery& WithComponent()
        {
            required_components.push_back(std::type_index(typeid(T)));
            MarkDirty();
            return *this;
        }

        /// Set a predicate filter (conditional participation)
        /// Only entities matching components AND predicate will be included
        EntityQuery& WithPredicate(Predicate pred)
        {
            predicate = pred;
            MarkDirty();
            return *this;
        }

        /// Mark cache as needing rebuild
        void MarkDirty() { dirty = true; }

        /// Initial scan: Rebuild cache by scanning all entities
        /// Used when a new System is added or filter changes
        /// Returns number of entities added
        size_t Rebuild();

        /// Directly add an entity to cache (if it matches signature + predicate)
        /// Called when entity gains a component
        /// Returns true if entity was added
        bool TryAddEntity(EntityID id, const Entity* entity);

        /// Directly remove an entity from cache
        /// Called when entity loses a component
        /// Returns true if entity was removed
        bool TryRemoveEntity(EntityID id);

        /// Remove stale cached entries that are no longer valid in context
        /// Returns number of removed entries
        size_t RemoveInvalidEntities();

        /// Get cached entities
        const std::vector<EntityID>& GetEntities() const
        {
            return cached_entities;
        }

        /// Get entity count
        size_t GetEntityCount() const
        {
            return cached_entities.size();
        }

        /// Check if a specific entity matches this query (signature + predicate)
        bool Matches(EntityID id) const;

        /// Check if entity matches component signature (ignores predicate)
        bool MatchesSignature(EntityID id) const;

        /// Clear cache
        void Clear()
        {
            cached_entities.clear();
            dirty = true;
        }

        /// Get required components
        const std::vector<std::type_index>& GetRequiredComponents() const
        {
            return required_components;
        }

        /// Check if query has no components (matches nothing)
        bool IsEmpty() const { return required_components.empty(); }

        /// Check if this query has a predicate filter
        bool HasPredicate() const { return predicate != nullptr; }
    };

    /**
     * SystemCache - Manages all queries for a specific system
     * Handles three participation modes:
     * 1. Reactive: Context pushes add/remove when entities gain/lose components
     * 2. Conditional: Predicates filter which entities actually enter cache
     * 3. Manual: System can directly add/remove entities for custom logic
     */
    class SystemCache
    {
    private:
        std::vector<std::unique_ptr<EntityQuery>> queries;
        ECSContext* context = nullptr;

    public:
        SystemCache(ECSContext* ctx) : context(ctx) {}

        /// Create a new query with component requirements
        template<typename FirstComponent, typename... RestComponents>
        EntityQuery* CreateQuery()
        {
            auto query = std::make_unique<EntityQuery>(context);
            _AddComponentsToQuery<FirstComponent, RestComponents...>(query.get());
            return queries.emplace_back(std::move(query)).get();
        }

        /// Initial scan: Rebuild all queries (only when system first added)
        void RebuildAll()
        {
            for (auto& query : queries)
            {
                query->MarkDirty();
                query->Rebuild();
            }
        }

        /// Reactive: Notify that an entity gained a component
        /// Will try to add entity to matching queries
        void OnComponentAdded(EntityID entity_id, const std::type_index& component_type, const Entity* entity);

        /// Reactive: Notify that an entity lost a component
        /// Will try to remove entity from affected queries
        void OnComponentRemoved(EntityID entity_id, const std::type_index& component_type);

        /// Reactive: Notify that an entity is being destroyed
        /// Ensures entity is removed from all query caches
        void OnEntityDestroyed(EntityID entity_id);

        /// Manual: Explicitly add entity to a query (for custom logic)
        void AddEntityManually(EntityQuery* query, EntityID entity_id, const Entity* entity);

        /// Manual: Explicitly remove entity from a query
        void RemoveEntityManually(EntityQuery* query, EntityID entity_id);

        /// Get number of queries
        size_t GetQueryCount() const { return queries.size(); }

        /// Remove stale entity IDs from all query caches
        /// Returns number of removed entries
        size_t RemoveInvalidEntities();

    private:
        template<typename FirstComponent>
        void _AddComponentsToQuery(EntityQuery* query)
        {
            query->WithComponent<FirstComponent>();
        }

        template<typename FirstComponent, typename SecondComponent, typename... RestComponents>
        void _AddComponentsToQuery(EntityQuery* query)
        {
            query->WithComponent<FirstComponent>();
            _AddComponentsToQuery<SecondComponent, RestComponents...>(query);
        }
    };
}

