#pragma once

#include <string>

namespace hgl::graph::contract {
class PhysicalDeviceProfileLite;
}

namespace hgl::graph::mtl {
struct StaticMaterialDef;
struct Material3DCreateConfig;
class MaterialCreateInfo;
}

namespace hgl::graph::mtl::internal {

MaterialCreateInfo *PrepareCompositorMaterialSnapshot(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    const Material3DCreateConfig *config,
    std::string *diagnostics);

std::string BuildShaderDataSchemaDebugText(const StaticMaterialDef &def);

} // namespace hgl::graph::mtl::internal