#pragma once
/// \file  SSBOOffsetHelper.h
/// \brief Pure arithmetic utilities for SSBO buffer usage flags and offset alignment.
///
/// All functions are constexpr / inline and have no Vulkan runtime dependency.
/// They can be unit-tested in a standalone executable without a Vulkan device.

#include <cstdint>

namespace hgl::graph
{

/// Stable Vulkan spec bit values for VkBufferUsageFlagBits (never change across spec versions).
inline constexpr uint32_t kVkBufferUsageStorageBufferBit = 0x00000020u;
inline constexpr uint32_t kVkBufferUsageVertexBufferBit  = 0x00000080u;

/// Compute VAB (VertexAttribBuffer) usage flags.
///
/// Always includes VERTEX_BUFFER_BIT so the buffer can be used as a traditional VBO.
/// When \p prefer_storage is true, STORAGE_BUFFER_BIT is also set so the same buffer
/// can be bound as an SSBO in a vertex-pulling pipeline without reallocation.
inline constexpr uint32_t ComputeVABUsageFlags(bool prefer_storage) noexcept
{
    return kVkBufferUsageVertexBufferBit | (prefer_storage ? kVkBufferUsageStorageBufferBit : 0u);
}

/// Align \p byte_offset upward to the next multiple of \p alignment that satisfies
/// minStorageBufferOffsetAlignment.
///
/// - If \p alignment is 0 the offset is valid as-is (no constraint).
/// - If \p byte_offset is already a multiple of \p alignment it is returned unchanged.
/// - Otherwise it is rounded up to the next multiple: (offset + align - 1) & ~(align - 1).
///
/// Pure arithmetic: no Vulkan device or header required.
inline constexpr uint64_t AlignStorageBufferOffset(uint64_t byte_offset,
                                                    uint64_t alignment) noexcept
{
    if (alignment == 0 || (byte_offset % alignment) == 0)
        return byte_offset;
    return (byte_offset + alignment - 1u) & ~(alignment - 1u);
}

} // namespace hgl::graph
