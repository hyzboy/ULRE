#pragma once

#include <hgl/graph/module/RendererShaderGenAdapter.h>

namespace hgl::graph
{
    namespace mtl
    {
        class MaterialCreateInfo;

        namespace contract
        {
            struct ShaderGenRequest;
            struct ShaderGenResult;
        }
    }

    bool RunReadOnlyValidationGate(const mtl::MaterialCreateInfo &mci,
                                   const mtl::contract::ShaderGenRequest *request_result,
                                   const mtl::contract::ShaderGenResult *mirror_result,
                                   const char *material_name,
                                   bool enable_mirror_validation,
                                   bool require_mirror_valid,
                                   RendererShaderGenAdapter::DiffLogDetail diff_log_detail);
}
