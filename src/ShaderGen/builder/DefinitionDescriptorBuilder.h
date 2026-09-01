#pragma once

#include <hgl/mtl/SerializedDescriptorEntry.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/common/RenderAssignDef.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include <hgl/mtl/ModuleResourceManifest.h>
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
        uint32_t(hgl::graph::kMeshFragment),
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
    // 顶点数据 SSBO（MeshShader 方向）：按需求语义注入顶点 SSBO 绑定
    // 顶点输入统一为 SSBO，无条件注入；PerObject 集固定 binding（s1_* 模块声明与固定名路径匹配）
    // 顶点数据 SSBO 注册（stage 可见性见下方 vertex_stage 的证据注释）
    {
        // 顶点数据 SSBO 只被 mesh 阶段读取——FS 拿的是 varying，不直读顶点流。
        // 证据（2026-08-31 全库 grep）：sbo_vertex_* 仅出现在 ShaderLibrary/vertex/s1_*.glsl
        // 与 mesh/line_quad.glsl.tmpl（均为 mesh 阶段），compositor/surface/material/
        // lighting/sky 零引用；且无任何 FS 源 #include "vertex/..."。
        // 对比：文本三 SSBO（TextCharInfo/Style/Instance）确实被 FS 直读
        //（material/text_source_gpu.glsl、common/surface_interface.glsl），故其
        // kMeshFragment 可见性是必需的——两者不可一概而论。
        const uint32_t vertex_stage = uint32_t(hgl::graph::ShaderStage::Mesh);
        bool need_uv = false, need_ntb = false, need_color = false, need_luminance = false, need_transform_id = false, need_size = false;
        for (int i = 0; i < definition.vertex_semantic_requirements.GetCount(); ++i)
        {
            const auto &requirement = definition.vertex_semantic_requirements[i];
            switch (requirement.semantic)
            {
            case GLSLCodeModuleSemantic::UV0: need_uv = true; break;
            case GLSLCodeModuleSemantic::Normal:
            case GLSLCodeModuleSemantic::Tangent:
            case GLSLCodeModuleSemantic::Binormal: need_ntb = true; break;
            case GLSLCodeModuleSemantic::Color: need_color = true; break;
            case GLSLCodeModuleSemantic::Luminance: need_luminance = true; break;
            case GLSLCodeModuleSemantic::TransformID: need_transform_id = true; break;
            case GLSLCodeModuleSemantic::Size: need_size = true; break;
            default: break;
            }
        }
        if (need_uv)
            descriptor_builder_common::PushVertexResource<DescriptorSemantic::VertexUV>(descriptors, vertex_stage);
        if (need_ntb)
            descriptor_builder_common::PushVertexResource<DescriptorSemantic::VertexNTB>(descriptors, vertex_stage);
        if (need_color)
            descriptor_builder_common::PushVertexResource<DescriptorSemantic::VertexColor>(descriptors, vertex_stage);
        if (need_luminance)
            descriptor_builder_common::PushVertexResource<DescriptorSemantic::VertexLuminance>(descriptors, vertex_stage);
        if (need_transform_id)
            descriptor_builder_common::PushVertexResource<DescriptorSemantic::VertexTransformID>(descriptors, vertex_stage);
        if (need_size)
            descriptor_builder_common::PushVertexResource<DescriptorSemantic::VertexSize>(descriptors, vertex_stage);
        // Position 恒有（S1 位置模块）
        descriptor_builder_common::PushVertexResource<DescriptorSemantic::VertexPosition>(descriptors, vertex_stage);
        // 索引恒有（s1_index 无条件 include——非索引绘制查表）
        descriptor_builder_common::PushVertexResource<DescriptorSemantic::VertexIndex>(descriptors, vertex_stage);
    }
    if (!descriptor_builder_common::AppendManifestSSBODescriptors(descriptors, manifest)
     || !descriptor_builder_common::AppendManifestTextureLayerDescriptors(descriptors, manifest))
        return {};
    descriptor_builder_common::EnsureMaterialPrivateDataIndexTable(
        descriptors, uint32_t(hgl::graph::kMeshFragment));

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
