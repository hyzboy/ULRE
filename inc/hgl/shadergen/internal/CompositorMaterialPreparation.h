#pragma once

#include <memory>
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

std::unique_ptr<MaterialCreateInfo> PrepareCompositorMaterialSnapshotOwned(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    const Material3DCreateConfig *config,
    std::string *diagnostics);

MaterialCreateInfo *PrepareCompositorMaterialSnapshot(
    const contract::PhysicalDeviceProfileLite *profile,
    const StaticMaterialDef &def,
    const std::string &vs_glsl,
    const std::string &fs_glsl,
    const Material3DCreateConfig *config,
    std::string *diagnostics);

std::string BuildShaderDataSchemaDebugText(const StaticMaterialDef &def);

} // namespace hgl::graph::mtl::internal