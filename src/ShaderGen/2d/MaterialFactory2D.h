#pragma once

#include <hgl/mtl/StaticMaterialDef.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <memory>
#include <string>

namespace hgl::graph::contract
{
struct PhysicalDeviceProfileLite;
}

namespace hgl::graph::mtl
{
class MaterialCreateInfo;
struct Material2DCreateConfig;

std::unique_ptr<MaterialCreateInfo> CreateFromFixedDef2DOwned(const char *debug_tag,
                                                              const contract::PhysicalDeviceProfileLite *profile,
                                                              const StaticMaterialDef &def,
                                                              const MaterialVariantKey &var_key,
                                                              const std::string &vs_preamble,
                                                              const std::string &fs_preamble,
                                                              const Material2DCreateConfig *cfg,
                                                              const MaterialVariantDesc &var_desc);
}
