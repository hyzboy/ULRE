#pragma once
#include <cstdint>

namespace hgl::graph::mtl
{
    // NOTE: MaterialKey stores versions as uint16.
    // Encoding: major<<8 | minor  (NOT VK_MAKE_API_VERSION full form)
    // kMaterialKeyVulkanVersion = 0x0102  →  VK 1.2
    // kMaterialKeySpvVersion    = 0x0105  →  SPV 1.5
    constexpr uint16_t kMaterialKeyGLSLVersion   = 450;    // GLSL 4.50
    constexpr uint16_t kMaterialKeyVulkanVersion = 0x0102; // VK 1.2
    constexpr uint16_t kMaterialKeySpvVersion    = 0x0105; // SPV 1.5

    // Schema version is bumped whenever MaterialKey layout changes.
    // v2: PositionProviderId renumbered (VAB 0x01-0xFF, PCG 0x0100+, UserPCG 0x1000)
    //     DirectVec3 renamed to VAB_Vec3; Unknown moved from 0x7FFF to 0x0000.
    //     All SPIR-V and VkPipelineCache blobs from schema v1 must be discarded.
    constexpr uint32_t kMaterialKeySchemaVersion = 2;

} // namespace hgl::graph::mtl
