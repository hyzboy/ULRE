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
    constexpr uint32_t kMaterialKeySchemaVersion = 1;

} // namespace hgl::graph::mtl
