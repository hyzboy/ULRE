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

    // A6-2a/b1：L2W/L2WIndex/mtl_data_addrs 行表不再经契约声明——l2w_ssbo 由模板侧
    // 无条件注入，l2w_index/ResolveTransformID 无条件发射，FS mtl_data_addrs 门按编译配置
    // material_private_data 直判；运行时按材质类别静态处理（无 L2W 材质 push nullptr 安全）。

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
    // A6-2b-b1：manifest 数据槽不再桥接契约行表条目——数据槽信号由编译配置
    // material_private_data 直判（ResolveEffectiveMaterialPrivateData 单槽合并），
    // 行表存在性/FS 发射门不再经契约。
    return BuildDescriptorsFromDefinition(definition, opt);
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
