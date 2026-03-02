#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>

namespace hgl::graph::mtl
{
    class MaterialCreateInfo;
}

namespace hgl::graph::mtl::contract
{
    bool BuildShaderGenResultFromMaterialCreateInfo(const MaterialCreateInfo &mci, ShaderGenResult &result);
}
