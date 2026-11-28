#pragma once

#include <hgl/graph/BufferAnnotation.h>
#include <hgl/math/Vector.h>
#include <cstddef>
#include <cstdint>

namespace hgl::graph::ssbo
{
    using hgl::math::Vector4f;

    struct alignas(16) HGL_SSBO_BDA MeshletDescriptorRow
    {
        uint32_t vertex_offset = 0;
        uint32_t triangle_offset = 0;
        uint32_t bounds_index = 0;
        uint32_t packed_counts = 0;
    };

    struct alignas(16) HGL_SSBO_BDA MeshletBoundsRow
    {
        Vector4f center_radius = Vector4f(0.0f);
        float cone_apex[3] = { 0.0f, 0.0f, 0.0f };
        uint32_t cone_axis_cutoff_packed = 0;
    };

    struct HGL_SSBO_BDA MeshletVertexRow
    {
        uint32_t vertex_index = 0;
    };

    struct HGL_SSBO_BDA MeshletTriangleRow
    {
        uint32_t packed_indices = 0;
    };

    static_assert(sizeof(MeshletDescriptorRow) == 16);
    static_assert(alignof(MeshletDescriptorRow) == 16);
    static_assert(sizeof(MeshletBoundsRow) == 32);
    static_assert(alignof(MeshletBoundsRow) == 16);
    static_assert(offsetof(MeshletBoundsRow, cone_apex) == 16);
    static_assert(offsetof(MeshletBoundsRow, cone_axis_cutoff_packed) == 28);
    static_assert(sizeof(MeshletVertexRow) == 4);
    static_assert(sizeof(MeshletTriangleRow) == 4);
}
