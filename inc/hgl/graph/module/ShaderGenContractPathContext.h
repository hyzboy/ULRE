#pragma once

#include <hgl/graph/module/RendererShaderGenAdapter.h>
#include <hgl/graph/module/ShaderGenPathMode.h>
#include <hgl/shadergen/contract/ShaderGenContract.h>

namespace hgl::graph
{
    class GraphicsContext;

    struct ShaderGenContractPathContext
    {
        ShaderGenPathMode mode = ShaderGenPathMode::MirrorValidate;
        ShaderGenPathPolicy policy = MakeShaderGenPathPolicy(ShaderGenPathMode::MirrorValidate);
        RendererShaderGenAdapter::DiffLogDetail diff_log_detail = RendererShaderGenAdapter::DiffLogDetail::SummaryOnly;

        mtl::contract::ShaderGenRequest request_storage;
        mtl::contract::ShaderGenResult mirror_storage;

        const mtl::contract::ShaderGenRequest *request = nullptr;
        const mtl::contract::ShaderGenResult *mirror = nullptr;

        bool mirror_prebuild_failed = false;
    };

    void BuildShaderGenContractPathContext(ShaderGenContractPathContext &ctx,
                                           const GraphicsContext *graphics_context,
                                           const mtl::MaterialCreateInfo &mci,
                                           const char *material_name);
}
