#include <hgl/graph/module/ShaderGenContractPathContext.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/shadergen/contract/ShaderGenRequestBuilder.h>
#include <hgl/shadergen/contract/ShaderGenResultBuilder.h>

namespace hgl::graph
{
    void BuildShaderGenContractPathContext(ShaderGenContractPathContext &ctx,
                                           const GraphicsContext *graphics_context,
                                           const mtl::MaterialCreateInfo &mci,
                                           const char *material_name)
    {
        ctx.mode = graphics_context ? graphics_context->GetShaderGenPathMode() : ShaderGenPathMode::MirrorValidate;
        ctx.policy = graphics_context ? graphics_context->GetShaderGenPathPolicy() : MakeShaderGenPathPolicy(ctx.mode);
        ctx.diff_log_detail = ctx.policy.full_diff_log
                            ? RendererShaderGenAdapter::DiffLogDetail::Full
                            : RendererShaderGenAdapter::DiffLogDetail::SummaryOnly;

        ctx.request = nullptr;
        ctx.mirror = nullptr;
        ctx.mirror_prebuild_failed = false;

        if (ctx.policy.enable_mirror_validation &&
            mtl::contract::BuildShaderGenRequestFromMaterialCreateInfo(mci, ctx.request_storage, material_name))
        {
            ctx.request = &ctx.request_storage;
        }

        if (ctx.policy.enable_mirror_validation &&
            mtl::contract::BuildShaderGenResultFromMaterialCreateInfo(mci, ctx.mirror_storage))
        {
            ctx.mirror = &ctx.mirror_storage;
        }
        else if (ctx.policy.enable_mirror_validation)
        {
            ctx.mirror_prebuild_failed = true;
        }
    }
}
