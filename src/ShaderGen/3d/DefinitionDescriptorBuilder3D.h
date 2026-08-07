#pragma once

#include <hgl/mtl/FixedDescriptorEntry.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/common/RenderAssignDef.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/graph/glsl/ShaderResourceManifest.h>
#include <vector>
#include "../common/DescriptorBuilderCommon.h"

namespace hgl::graph::mtl
{

struct Build3DDescriptorOptions
{
    uint32_t sky_stage_flags = uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS);
    uint32_t color_palette_stage_flags = uint32_t(VK_SHADER_STAGE_VERTEX_BIT);
    uint32_t texture_stage_flags = uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT);
    uint32_t material_texture_layer_table_stage_flags = uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS);
};

inline std::vector<FixedDescriptorEntry> Build3DDescriptorsFromDefinition(
    const MaterialDefinition &definition,
    const Build3DDescriptorOptions &opt = {})
{
    std::vector<FixedDescriptorEntry> descriptors;
    descriptors.reserve(16);

    descriptor_builder_common::AppendDefinitionUBODescriptors(
        descriptors,
        definition,
        uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
        opt.sky_stage_flags,
        opt.color_palette_stage_flags);

    if (definition.vertex_node_config.projection != ProjectionMode::OrthoViewport
     && definition.vertex_node_config.projection != ProjectionMode::ClipPassthrough)
    {
        descriptor_builder_common::PushLocalToWorld(descriptors, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
        descriptor_builder_common::PushLocalToWorldIndexRows(descriptors, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
    }

    descriptor_builder_common::AppendDefinitionMaterialDescriptors(
        descriptors,
        definition,
        uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
        opt.material_texture_layer_table_stage_flags,
        opt.texture_stage_flags);

    return descriptors;
}

inline bool Build3DShaderResourceManifest(
    const MaterialDefinition &definition,
    const SkyLightAmbientModel ambient_model,
    ShaderResourceManifest &manifest)
{
    bool has_sky_root = false;
    for (const GLSLCodeModuleID id : definition.code_module_requirements)
    {
        if (id == GLSLCodeModuleID::SkyLightHeader
         || id == GLSLCodeModuleID::SkyLightSimple
         || id == GLSLCodeModuleID::SkyLightCubeMap)
        {
            has_sky_root = true;
            break;
        }
    }

    GLSLCodeModuleID ambient_root = GLSLCodeModuleID::SkyLightSimple;
    if (has_sky_root)
    {
        ambient_root = ambient_model == SkyLightAmbientModel::CubeMap
            ? GLSLCodeModuleID::SkyLightCubeMap
            : GLSLCodeModuleID::SkyLightSimple;
    }

    return descriptor_builder_common::BuildDefinitionShaderResourceManifest(
        definition,
        manifest,
        has_sky_root ? &ambient_root : nullptr,
        has_sky_root ? 1u : 0u);
}

inline std::vector<FixedDescriptorEntry> Build3DDescriptorsFromDefinition(
    const MaterialDefinition &definition,
    ShaderResourceManifest &manifest,
    const Build3DDescriptorOptions &opt = {})
{
    std::vector<FixedDescriptorEntry> descriptors = Build3DDescriptorsFromDefinition(definition, opt);
    descriptor_builder_common::AppendManifestUBODescriptors(descriptors, manifest);
    if (!descriptor_builder_common::AppendManifestSSBODescriptors(descriptors, manifest)
     || !descriptor_builder_common::AppendManifestTextureDescriptors(descriptors, manifest))
        return {};

    return descriptors;
}

inline std::vector<FixedDescriptorEntry> Build3DDescriptorsFromDefinition(
    const MaterialDefinition &definition,
    const ShaderResourceManifest &manifest,
    const Build3DDescriptorOptions &opt = {})
{
    ShaderResourceManifest mutable_manifest = manifest;
    return Build3DDescriptorsFromDefinition(definition, mutable_manifest, opt);
}

} // namespace hgl::graph::mtl
