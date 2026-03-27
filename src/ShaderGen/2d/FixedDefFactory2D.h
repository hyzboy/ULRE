#pragma once

#include <hgl/mtl/FixedMaterialDef.h>
#include <hgl/mtl/new/MaterialVariantKey.h>
#include <string>

namespace hgl::graph::contract
{
struct PhysicalDeviceProfileLite;
}

namespace hgl::graph::mtl
{
class MaterialCreateInfo;
struct Material2DCreateConfig;

MaterialCreateInfo *CreateFromFixedDef2D(const char *debug_tag,
                                         const contract::PhysicalDeviceProfileLite *profile,
                                         const FixedMaterialDef &def,
                                         const MaterialVariantKey &var_key,
                                         const std::string &vs_preamble,
                                         const std::string &fs_preamble,
                                         const Material2DCreateConfig *cfg,
                                         const bool use_canonical_fallback = false);
}
