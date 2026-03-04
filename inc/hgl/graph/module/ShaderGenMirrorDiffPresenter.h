#pragma once

#include <hgl/graph/module/ShaderGenDiffLogDetail.h>
#include <hgl/graph/module/ShaderGenValidationTypes.h>

namespace hgl::graph
{
    namespace mtl
    {
        class MaterialCreateInfo;

        namespace contract
        {
            struct ShaderGenResult;
        }
    }

    bool PresentShaderGenMirrorDiff(const mtl::MaterialCreateInfo &mci,
                                    const mtl::contract::ShaderGenResult &result,
                                    const char *material_name,
                                    ShaderGenDiffLogDetail detail,
                                    ShaderGenValidationReport *out_report);
}
