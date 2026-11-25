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
        /// Generation for detecting stale handles。必须是 32 位：（index 4B + generation 4B）
        /// 仍只占 8 字节（原 uint16_t 版本因对齐带 2B padding，等于白占），
        /// 却把 ABA 回绕阈值从 6.5 万次同槽复用抬到 42.9 亿次。
        uint32_t generation = 0;

        EntityID() = default;
        constexpr EntityID(uint32_t idx, uint32_t gen = 0)
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
            return (static_cast<size_t>(id.index) << 32) | static_cast<size_t>(id.generation);
        }
    };
}
