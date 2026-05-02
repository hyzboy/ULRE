#pragma once

#include <hgl/common/AttributeProvider.h>
#include <cstdint>

namespace hgl::graph
{
    // VertexStreams set uses sparse binding indices:
    // [0..7] attribute streams, [8] position, [9+] mesh shader specific streams.
    constexpr uint32_t kVertexStreamAttributeBindingBegin = 0u;
    constexpr uint32_t kVertexStreamAttributeBindingCount = uint32_t(AttributeSemantic::BuiltinCount);
    constexpr uint32_t kVertexStreamPositionBinding = kVertexStreamAttributeBindingCount;

    constexpr uint32_t kMeshShaderIndexStreamBinding       = kVertexStreamPositionBinding + 1u;
    constexpr uint32_t kMeshShaderMeshletStreamBinding     = kVertexStreamPositionBinding + 2u;
    constexpr uint32_t kMeshShaderTaskPayloadStreamBinding = kVertexStreamPositionBinding + 3u;

    constexpr uint32_t kVertexStreamBindingCountWithMesh   = kMeshShaderTaskPayloadStreamBinding + 1u;

    struct MeshShaderStreamLayout
    {
        uint32_t binding = 0;
        uint32_t byte_stride = 0;
    };

    struct MeshShaderStreamContract
    {
        // Default index stream layout assumes uint32 triangle/primitive index data.
        MeshShaderStreamLayout index{kMeshShaderIndexStreamBinding, 4u};

        // Optional streams are reserved for phased rollout.
        MeshShaderStreamLayout meshlet{kMeshShaderMeshletStreamBinding, 0u};
        MeshShaderStreamLayout task_payload{kMeshShaderTaskPayloadStreamBinding, 0u};

        bool enable_meshlet_stream = false;
        bool enable_task_payload_stream = false;
    };

    constexpr MeshShaderStreamContract MakeDefaultMeshShaderStreamContract() noexcept
    {
        return MeshShaderStreamContract{};
    }

    static_assert(kVertexStreamPositionBinding == uint32_t(AttributeSemantic::BuiltinCount));
    static_assert(kVertexStreamBindingCountWithMesh == (uint32_t(AttributeSemantic::BuiltinCount) + 4u));
}
