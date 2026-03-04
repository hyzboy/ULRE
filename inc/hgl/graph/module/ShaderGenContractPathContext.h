#pragma once

#include <hgl/graph/module/ShaderGenDiffLogDetail.h>
#include <hgl/graph/module/ShaderGenPathMode.h>
#include <hgl/shadergen/contract/ShaderGenContract.h>
#include <functional>

namespace hgl::graph
{
    class GraphicsContext;

    namespace mtl
    {
        class MaterialCreateInfo;
    }

    struct ShaderGenContractPathContext
    {
        ShaderGenPathMode mode = ShaderGenPathMode::MirrorValidate;
        ShaderGenPathPolicy policy = MakeShaderGenPathPolicy(ShaderGenPathMode::MirrorValidate);
        ShaderGenDiffLogDetail diff_log_detail = ShaderGenDiffLogDetail::SummaryOnly;

        mtl::contract::PhysicalDeviceProfileLite physical_device_profile_storage;
        const mtl::contract::PhysicalDeviceProfileLite *physical_device_profile = nullptr;

        mtl::contract::ShaderGenRequest request_storage;
        mtl::contract::ShaderGenResult mirror_storage;

        const mtl::contract::ShaderGenRequest *request = nullptr;
        const mtl::contract::ShaderGenResult *mirror = nullptr;

        bool mirror_prebuild_failed = false;
    };

    using ShaderGenRequestBuilderFn = std::function<bool(const mtl::MaterialCreateInfo &,
                                                         mtl::contract::ShaderGenRequest &,
                                                         const char *material_name)>;

    using ShaderGenResultBuilderFn = std::function<bool(const mtl::MaterialCreateInfo &,
                                                        mtl::contract::ShaderGenResult &)>;

    void BuildShaderGenContractPathContextWithBuilders(ShaderGenContractPathContext &ctx,
                                                       const GraphicsContext *graphics_context,
                                                       const mtl::MaterialCreateInfo &mci,
                                                       const char *material_name,
                                                       const ShaderGenRequestBuilderFn &request_builder,
                                                       const ShaderGenResultBuilderFn &result_builder,
                                                       const mtl::contract::PhysicalDeviceProfileLite *preferred_profile = nullptr);

    void BuildShaderGenContractPathContext(ShaderGenContractPathContext &ctx,
                                           const GraphicsContext *graphics_context,
                                           const mtl::MaterialCreateInfo &mci,
                                           const char *material_name);
}
