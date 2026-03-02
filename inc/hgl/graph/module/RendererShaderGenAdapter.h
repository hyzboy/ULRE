#pragma once

#include <hgl/shadergen/contract/ShaderGenContract.h>

namespace hgl::graph
{
    namespace mtl
    {
        class MaterialCreateInfo;
    }

    class RendererShaderGenAdapter
    {
    public:

        bool ConsumeResultReadOnly(const mtl::contract::ShaderGenResult &result, const char *material_name) const;
        bool ConsumeMaterialReadOnly(const mtl::MaterialCreateInfo &mci, const char *material_name) const;
    };
}//namespace hgl::graph
