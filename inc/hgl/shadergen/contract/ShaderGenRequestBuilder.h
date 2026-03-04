#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>

namespace hgl::graph::mtl
{
    class MaterialCreateInfo;
}

namespace hgl::graph::mtl::contract
{
    bool BuildShaderGenRequestFromMaterialCreateInfo(const MaterialCreateInfo &mci,
                                                     const PhysicalDeviceProfileLite *physical_device_profile,
                                                     ShaderGenRequest &request,
                                                     const char *material_name = nullptr);

    bool BuildShaderGenRequestFromMaterialCreateInfo(const MaterialCreateInfo &mci, ShaderGenRequest &request, const char *material_name = nullptr);
}
