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

        enum class DiffLogDetail
        {
            SummaryOnly,
            Full,
        };

        bool ConsumePairReadOnly(const mtl::MaterialCreateInfo &mci, const mtl::contract::ShaderGenResult &result, const char *material_name, DiffLogDetail detail = DiffLogDetail::Full) const;
        bool ConsumeResultReadOnly(const mtl::contract::ShaderGenResult &result, const char *material_name) const;
        bool ConsumeMaterialReadOnly(const mtl::MaterialCreateInfo &mci, const char *material_name, DiffLogDetail detail = DiffLogDetail::Full) const;
    };
}//namespace hgl::graph
