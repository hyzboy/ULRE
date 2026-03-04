#include <hgl/graph/module/ShaderGenContractPathContext.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/shadergen/contract/ShaderGenRequestBuilder.h>
#include <hgl/shadergen/contract/ShaderGenResultBuilder.h>
#include <hgl/shadergen/contract/ShaderGenPhysicalDeviceProfileAdapter.h>
#include <hgl/vk/VKDeviceAttribute.h>

namespace hgl::graph
{
    void BuildShaderGenContractPathContextWithBuilders(ShaderGenContractPathContext &ctx,
                                                       const GraphicsContext *graphics_context,
                                                       const mtl::MaterialCreateInfo &mci,
                                                       const char *material_name,
                                                       const ShaderGenRequestBuilderFn &request_builder,
                                                       const ShaderGenResultBuilderFn &result_builder,
                                                       const mtl::contract::PhysicalDeviceProfileLite *preferred_profile)
    {
        ctx.mode = graphics_context ? graphics_context->GetShaderGenPathMode() : ShaderGenPathMode::MirrorValidate;
        ctx.policy = graphics_context ? graphics_context->GetShaderGenPathPolicy() : MakeShaderGenPathPolicy(ctx.mode);
        ctx.diff_log_detail = ctx.policy.full_diff_log
                            ? ShaderGenDiffLogDetail::Full
                            : ShaderGenDiffLogDetail::SummaryOnly;

        ctx.physical_device_profile = nullptr;
        if (preferred_profile)
        {
            ctx.physical_device_profile_storage = *preferred_profile;
            ctx.physical_device_profile = &ctx.physical_device_profile_storage;
        }
        else if (graphics_context)
        {
            const VulkanDevAttr *dev_attr = graphics_context->GetDevAttr();
            if (dev_attr && dev_attr->physical_device)
            {
                ctx.physical_device_profile_storage =
                    mtl::contract::BuildPhysicalDeviceProfileFromVulkanPhyDevice(*dev_attr->physical_device);
                ctx.physical_device_profile = &ctx.physical_device_profile_storage;
            }
        }

        ctx.request = nullptr;
        ctx.mirror = nullptr;
        ctx.mirror_prebuild_failed = false;

        if (ctx.policy.enable_mirror_validation &&
            request_builder &&
            request_builder(mci, ctx.request_storage, material_name))
        {
            if (ctx.physical_device_profile && !ctx.request_storage.has_physical_device_profile)
            {
                ctx.request_storage.has_physical_device_profile = true;
                ctx.request_storage.physical_device_profile = *ctx.physical_device_profile;
            }

            ctx.request = &ctx.request_storage;
        }

        if (ctx.policy.enable_mirror_validation &&
            result_builder &&
            result_builder(mci, ctx.mirror_storage))
        {
            ctx.mirror = &ctx.mirror_storage;
        }
        else if (ctx.policy.enable_mirror_validation)
        {
            ctx.mirror_prebuild_failed = true;
        }
    }

    void BuildShaderGenContractPathContext(ShaderGenContractPathContext &ctx,
                                           const GraphicsContext *graphics_context,
                                           const mtl::MaterialCreateInfo &mci,
                                           const char *material_name)
    {
        BuildShaderGenContractPathContextWithBuilders(
            ctx,
            graphics_context,
            mci,
            material_name,
            [&ctx](const mtl::MaterialCreateInfo &src,
               mtl::contract::ShaderGenRequest &request,
               const char *name)->bool
            {
                return mtl::contract::BuildShaderGenRequestFromMaterialCreateInfo(src,
                                                                                  ctx.physical_device_profile,
                                                                                  request,
                                                                                  name);
            },
            [](const mtl::MaterialCreateInfo &src,
               mtl::contract::ShaderGenResult &result)->bool
            {
                return mtl::contract::BuildShaderGenResultFromMaterialCreateInfo(src, result);
            },
            nullptr);
    }
}
