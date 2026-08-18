#pragma once

#include <hgl/mtl/SerializedDescriptorEntry.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/common/RenderAssignDef.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/mtl/ModuleResourceManifest.h>
#include <vector>
#include "../common/DescriptorBuilderCommon.h"

namespace hgl::graph::mtl
{
struct BuildDescriptorOptions
{
    uint32_t sky_stage_flags = uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS);
    uint32_t color_palette_stage_flags = uint32_t(VK_SHADER_STAGE_VERTEX_BIT);
    uint32_t material_texture_layer_table_stage_flags = uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS);
};

inline std::vector<SerializedDescriptorEntry> BuildDescriptorsFromDefinition(
    const MaterialDefinition &definition,
    const BuildDescriptorOptions &opt = {})
{
    std::vector<SerializedDescriptorEntry> descriptors;
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
        opt.material_texture_layer_table_stage_flags);

    return descriptors;
}

inline bool BuildModuleResourceManifest(
    const MaterialDefinition &definition,
    ModuleResourceManifest &manifest,
    const char *const *provider_roots = nullptr,
    const uint32 provider_root_count = 0,
    const GLSLCodeModuleRegistry *registry = nullptr)
{
    return descriptor_builder_common::BuildDefinitionModuleResourceManifest(
        definition, manifest, provider_roots, provider_root_count, registry);
}

inline std::vector<SerializedDescriptorEntry> BuildDescriptorsFromDefinition(
    const MaterialDefinition &definition,
    ModuleResourceManifest &manifest,
    const BuildDescriptorOptions &opt = {})
{
    std::vector<SerializedDescriptorEntry> descriptors = BuildDescriptorsFromDefinition(definition, opt);
    descriptor_builder_common::AppendManifestUBODescriptors(descriptors, manifest);
    if (!descriptor_builder_common::AppendManifestSSBODescriptors(descriptors, manifest)
     || !descriptor_builder_common::AppendManifestTextureLayerDescriptors(descriptors, manifest))
        return {};
    descriptor_builder_common::EnsureMaterialDataIndexTable(
        descriptors, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));

    return descriptors;
}

inline std::vector<SerializedDescriptorEntry> BuildDescriptorsFromDefinition(
    const MaterialDefinition &definition,
    const ModuleResourceManifest &manifest,
    const BuildDescriptorOptions &opt = {})
{
    ModuleResourceManifest mutable_manifest = manifest;
    return BuildDescriptorsFromDefinition(definition, mutable_manifest, opt);
}

} // namespace hgl::graph::mtl
