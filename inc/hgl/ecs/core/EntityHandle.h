#pragma once

#include <cstdint>
#include <functional>
#include <limits>

namespace hgl::ecs
{
    /// EntityID - lightweight handle to an entity
    /// Combines index and generation for safe entity lookup
    struct EntityID
    {
        uint32_t index = UINT32_MAX;      ///< Index in the entity pool
        uint16_t generation = 0;          ///< Generation for detecting stale handles
        uint16_t reserved = 0;            ///< Reserved for future use
        
        EntityID() = default;
        constexpr EntityID(uint32_t idx, uint16_t gen = 0) 
            : index(idx), generation(gen) {}
        
        /// Check if this ID is valid
        constexpr bool IsValid() const { return index != UINT32_MAX; }
        
        /// Create invalid ID
        constexpr static EntityID Invalid() { return EntityID(UINT32_MAX, 0); }
        
        constexpr bool operator==(const EntityID& other) const 
        {
            return index == other.index && generation == other.generation;
        }
        
        constexpr bool operator!=(const EntityID& other) const 
        {
            return !(*this == other);
        }
        
        constexpr bool operator<(const EntityID& other) const
        {
            if (index != other.index) return index < other.index;
            return generation < other.generation;
        }
    };
}

namespace std
{
    template<>
    struct hash<hgl::ecs::EntityID>
    {
        size_t operator()(const hgl::ecs::EntityID& id) const
        {
            return ((size_t)id.index << 16) | (size_t)id.generation;
        }
    };
}
