#pragma once

#include <hgl/mtl/SerializedDescriptorEntry.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/common/RenderAssignDef.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <vector>
#include "DescriptorBuilderCommon.h"

namespace hgl::graph::mtl
{
struct BuildDescriptorOptions
{
    uint32_t sky_stage_flags = uint32_t(hgl::graph::kMeshFragment);
    uint32_t color_palette_stage_flags = uint32_t(hgl::graph::kMeshFragment);
    uint32_t material_texture_layer_table_stage_flags = uint32_t(hgl::graph::kMeshFragment);
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
        uint32_t(hgl::graph::kMeshFragment),
        opt.sky_stage_flags,
        opt.color_palette_stage_flags);

    if (definition.vertex_node_config.projection != ProjectionMode::OrthoViewport
     && definition.vertex_node_config.projection != ProjectionMode::ClipPassthrough)
    {
        descriptor_builder_common::PushLocalToWorld(descriptors, hgl::graph::kMeshFragment);
        descriptor_builder_common::PushLocalToWorldIndexRows(descriptors, hgl::graph::kMeshFragment);
    }

    descriptor_builder_common::AppendDefinitionMaterialDescriptors(
        descriptors,
        definition,
        uint32_t(hgl::graph::kMeshFragment));

    return descriptors;
}

inline bool BuildShaderCodeResourceManifest(
    const MaterialDefinition &definition,
    ShaderCodeResourceManifest &manifest,
    const char *const *provider_roots = nullptr,
    const uint32 provider_root_count = 0,
    const ShaderCodeModuleRegistry *registry = nullptr)
{
    return descriptor_builder_common::BuildDefinitionShaderCodeResourceManifest(
        definition, manifest, provider_roots, provider_root_count, registry);
}

inline std::vector<SerializedDescriptorEntry> BuildDescriptorsFromDefinition(
    const MaterialDefinition &definition,
    ShaderCodeResourceManifest &manifest,
    const BuildDescriptorOptions &opt = {})
{
    std::vector<SerializedDescriptorEntry> descriptors = BuildDescriptorsFromDefinition(definition, opt);
    // 顶点数据 SSBO（MeshShader 方向）：按需求语义注入顶点 SSBO 绑定
    // 顶点数据 SSBO 已随 Vertex 集退场（顶点流 BDA 化）——顶点数据经 MeshDrawParams
    // 行内基址到达 shader，描述符契约不再包含任何 Vertex 行。
    if (!descriptor_builder_common::AppendManifestSSBODescriptors(descriptors, manifest))
        return {};

    return descriptors;
}

inline std::vector<SerializedDescriptorEntry> BuildDescriptorsFromDefinition(
    const MaterialDefinition &definition,
    const ShaderCodeResourceManifest &manifest,
    const BuildDescriptorOptions &opt = {})
{
    ShaderCodeResourceManifest mutable_manifest = manifest;
    return BuildDescriptorsFromDefinition(definition, mutable_manifest, opt);
}

} // namespace hgl::graph::mtl
