#pragma once

/// Build2DCommon.h — 2D 材质转换公共辅助
///
/// 提供 2D FS preamble、DEF 构建等工具，
/// 供各 M_Xxx2D.cpp 工厂函数使用。
/// 2D 专用 fragment shader 已迁移至 Compositor 统一架构（参见 ShaderLibrary/compositor/ 与 ShaderLibrary/surface/）。

#include <hgl/mtl/FixedVertexEntry.h>
#include <hgl/mtl/FixedDescriptorEntry.h>
#include<hgl/common/RenderAssignDef.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include "../common/DescriptorBuilderCommon.h"
#include "../common/VertexBuilderCommon.h"
#include<string>
#include<vector>

namespace hgl::graph::mtl{

/// 2D 材质内部构建参数——从 MaterialDefinitionBuildRequest/MaterialDefinition 转换而来。
struct Material2DBuildParams
{
    PrimitiveType           prim                = PrimitiveType::Triangles;
    VertexShaderNodeConfig   vertex_node_config  = MakeDefault3DNodeConfig();
    uint32_t                shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);
    const GeometryVertexFormat *geometry_vertex_format = nullptr;
    const std::vector<MaterialDataSlotDecl> *data_slot_decls = nullptr;

    const MaterialDefinition *material_definition = nullptr;

    static Material2DBuildParams From(const MaterialDefinitionBuildRequest &request,
                                      const MaterialDefinition &definition)
    {
        Material2DBuildParams p;
        p.prim              = request.primitive_type;
        p.vertex_node_config = request.recipe.vertex_node_config;
        if (IsDefault3DNodeConfig(p.vertex_node_config) && !IsDefault3DNodeConfig(definition.vertex_node_config))
            p.vertex_node_config = definition.vertex_node_config;
        if(request.override_shader_stage_bits)
            p.shader_stage_flag_bit = request.shader_stage_flag_bit;
        p.geometry_vertex_format            = request.geometry_vertex_format;
        p.data_slot_decls = definition.data_slot_decls.empty() ? nullptr : &definition.data_slot_decls;
        p.material_definition = &definition;
        if(request.override_rt_output)
        {
            // rt_output override not stored in Material2DBuildParams (not needed by build helpers)
        }
        return p;
    }
};

namespace build2d{

inline std::string Build2DFragmentPreamble(
    const Material2DBuildParams &p,
    bool has_texture)
{
    (void)p;
    (void)has_texture;
    return "#version 450\n\n";
}

// ─────────────────────────────────────────────────────────────
// Common FixedVertexEntry builders
// ─────────────────────────────────────────────────────────────

inline void PushBaseVertexEntries(std::vector<FixedVertexEntry> &v, const Material2DBuildParams &p, VkFormat position_format_override = VK_FORMAT_UNDEFINED)
{
    const VkFormat position_format = (position_format_override!=VK_FORMAT_UNDEFINED)
                                   ? position_format_override
                                   : ResolveMaterialPositionFormat(p.geometry_vertex_format, VK_FORMAT_R32G32_SFLOAT);
    const vertex_builder_common::VertexSemanticDecl decls[] = {
        { VertexSemantic::Position, position_format, true, false },
    };
    const vertex_builder_common::VertexBuildInput input {
        p.prim,
        p.geometry_vertex_format,
        decls,
        1
    };
    vertex_builder_common::BuildVertexEntries(v, input);
}

inline void PushSemanticVertexEntry(std::vector<FixedVertexEntry> &v,
                                    const Material2DBuildParams &p,
                                    const VertexSemantic semantic,
                                    const VkFormat fallback_format)
{
    const vertex_builder_common::VertexSemanticDecl decls[] = {
        { semantic, fallback_format, false, false },
    };
    const vertex_builder_common::VertexBuildInput input {
        p.prim,
        p.geometry_vertex_format,
        decls,
        1
    };
    vertex_builder_common::BuildVertexEntries(v, input);
}

// ─────────────────────────────────────────────────────────────
// Common FixedDescriptorEntry builders
// ─────────────────────────────────────────────────────────────

inline void PushBaseDescriptorEntries(std::vector<FixedDescriptorEntry> &v, const Material2DBuildParams &p)
{
    // Viewport (Scene set) — only for Ortho
    if(p.vertex_node_config.projection == ProjectionMode::OrthoViewport
    || p.vertex_node_config.projection == ProjectionMode::OrthoThenLocalToWorld)
        descriptor_builder_common::PushViewport(v, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));

    // L2W (Transform set) — only if L2W
    if(p.vertex_node_config.projection == ProjectionMode::LocalToWorldOnly
    || p.vertex_node_config.projection == ProjectionMode::OrthoThenLocalToWorld
    || p.vertex_node_config.projection == ProjectionMode::WorldCameraVP)
    {
        descriptor_builder_common::PushLocalToWorld(v, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
        descriptor_builder_common::PushLocalToWorldIndexRows(v, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
    }

    if (p.material_definition)
    {
        descriptor_builder_common::AppendDefinitionUBODescriptors(
            v,
            *p.material_definition,
            uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
            uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
            uint32_t(VK_SHADER_STAGE_VERTEX_BIT));
        descriptor_builder_common::AppendDefinitionMaterialDescriptors(
            v,
            *p.material_definition,
            uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
            uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
            uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT));
    }
}

inline bool Build2DShaderResourceManifest(
    const MaterialDefinition &definition,
    ShaderResourceManifest &manifest,
    const GLSLCodeModuleID *provider_roots = nullptr,
    const uint32 provider_root_count = 0,
    const GLSLCodeModuleRegistry *registry = nullptr)
{
    return descriptor_builder_common::BuildDefinitionShaderResourceManifest(
        definition, manifest, provider_roots, provider_root_count, registry);
}

inline std::vector<FixedDescriptorEntry> Build2DDescriptorsFromDefinition(
    const Material2DBuildParams &p,
    ShaderResourceManifest &manifest)
{
    std::vector<FixedDescriptorEntry> descriptors;
    PushBaseDescriptorEntries(descriptors, p);
    descriptor_builder_common::AppendManifestUBODescriptors(descriptors, manifest);
    if (!descriptor_builder_common::AppendManifestSSBODescriptors(descriptors, manifest)
     || !descriptor_builder_common::AppendManifestTextureDescriptors(descriptors, manifest)
     || !descriptor_builder_common::AppendManifestTextureLayerDescriptors(descriptors, manifest))
        return {};
    descriptor_builder_common::EnsureMaterialDataIndexTable(
        descriptors, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));

    return descriptors;
}

inline std::vector<FixedDescriptorEntry> Build2DDescriptorsFromDefinition(
    const Material2DBuildParams &p,
    const ShaderResourceManifest &manifest)
{
    ShaderResourceManifest mutable_manifest = manifest;
    return Build2DDescriptorsFromDefinition(p, mutable_manifest);
}

// ─────────────────────────────────────────────────────────────
// Convert Material2DBuildParams → CompositorMaterialBuildConfig (2D: no camera/sky)
// ─────────────────────────────────────────────────────────────

inline CompositorMaterialBuildConfig ToCompositorBuildConfig2D(const Material2DBuildParams &p)
{
    CompositorMaterialBuildConfig bc;
    bc.primitive_type                  = p.prim;
    bc.shader_stage_flag_bits          = p.shader_stage_flag_bit;
    bc.data_slot_decls                 = p.data_slot_decls;
    bc.material_definition             = p.material_definition;
    return bc;
}

}//namespace build2d
}//namespace hgl::graph::mtl
