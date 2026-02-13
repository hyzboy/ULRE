#pragma once

#include<hgl/ecs/EntityHandle.h>
#include <ankerl/unordered_dense.h>
#include<mutex>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * VisibilityDataStorage - Fast lookup for invisible entities with hierarchical support
     * 
     * Maintains a set of invisible entity IDs for O(1) query during rendering.
     * Updated directly by VisibilityComponent when visibility changes.
     * 
     * Supports hierarchical visibility: if an ancestor is invisible, all descendants are invisible.
     */
    class VisibilityDataStorage
    {
    private:
        ankerl::unordered_dense::set<EntityID> invisible_entities;
        mutable std::mutex mutex;
        ECSContext* context = nullptr;

    public:
        VisibilityDataStorage(ECSContext* ctx = nullptr) : context(ctx) {}
        ~VisibilityDataStorage() = default;

        /// Set the context for hierarchical visibility checks
        void SetContext(ECSContext* ctx) { context = ctx; }

        /// Mark entity as invisible
        void SetInvisible(EntityID entity_id);

        /// Mark entity as visible
        void SetVisible(EntityID entity_id);

        /// Check if entity is invisible (O(1) lookup)
        /// Also checks ancestor chain for hierarchical visibility
        bool IsInvisible(EntityID entity_id) const;

        /// Query only direct visibility (doesn't check ancestors)
        bool IsDirectlyInvisible(EntityID entity_id) const;

        /// Get count of invisible entities
        size_t GetInvisibleCount() const;

        /// Clear all data
        void Clear();
    };
}//namespace hgl::ecs
