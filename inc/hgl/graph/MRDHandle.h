#pragma once

#include <cstdint>
#include <functional>

namespace hgl::graph {

/**
 * 强类型资源域句柄（值语义，可按值传递和存储）。
 *
 * id == 0 表示无效；generation 防止 use-after-release 悬空访问。
 * 持有者可以无限制地复制、比较和哈希该句柄；
 * 真正的 InstanceDataDomain* 由 MRDManager::Get(handle) 按需解引用。
 */
struct MRDHandle
{
    uint32_t id         = 0;
    uint32_t generation = 0;

    bool IsValid() const noexcept { return id != 0; }

    bool operator==(const MRDHandle &o) const noexcept { return id == o.id && generation == o.generation; }
    bool operator!=(const MRDHandle &o) const noexcept { return !(*this == o); }
};

inline constexpr MRDHandle InvalidMRDHandle = {};

} // namespace hgl::graph

// ---------------------------------------------------------------------------
// std::hash specialization — hgl::graph::MRDHandle
// ---------------------------------------------------------------------------
namespace std {
template <>
struct hash<hgl::graph::MRDHandle>
{
    size_t operator()(const hgl::graph::MRDHandle &h) const noexcept
    {
        // Pack both words into one 64-bit key for a single hash call
        return std::hash<uint64_t>{}(
            (static_cast<uint64_t>(h.id) << 32) | static_cast<uint64_t>(h.generation));
    }
};
} // namespace std
