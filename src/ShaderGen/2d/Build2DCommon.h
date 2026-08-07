#pragma once

/// Build2DCommon.h — 2D 材质转换公共辅助
///
/// 提供 2D FS preamble、DEF 构建等工具，
/// 供各 M_Xxx2D.cpp 工厂函数使用。
/// GLSL 代码已移至 ShaderLibrary/2d/ 目录下的文件。

#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/common/RenderAssignDef.h>
#include <hgl/graph/ssbo/MaterialSSBOLayout.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include "../common/DescriptorBuilderCommon.h"
#include "../common/VertexBuilderCommon.h"
#include <cstring>
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

    // MaterialDataSlot — definition-driven (E4): driven by data_slot_decls, same as 3D path.
    if (p.material_definition && !p.material_definition->data_slot_decls.empty())
    {
        for (uint32_t i = 0; i < static_cast<uint32_t>(p.material_definition->data_slot_decls.size()); ++i)
        {
            const auto &decl = p.material_definition->data_slot_decls[i];
            descriptor_builder_common::PushMaterialDataSlot(
                v,
                uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS),
                decl.name.c_str(),
                ssbo::GetMaterialSSBOStructName(decl.ssbo_type),
                i,
                decl.ssbo_type);
        }
        descriptor_builder_common::PushMaterialDataIndexRows(v, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
    }

    // Definition-driven texture/sampler slots (E3+)
    if (p.material_definition)
    {
        for (const auto &decl : p.material_definition->texture_slot_decls)
        {
            const char *name = decl.name ? decl.name : descriptor_builder_common::GetTextureNameBySlot(decl.slot);
            descriptor_builder_common::PushMaterialSampler(
                v, name, decl.slot,
                ToGLSLSamplerTypeName(decl.sampler_type),
                uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT));
        }
    }
}

inline bool Build2DShaderResourceManifest(
    const MaterialDefinition &definition,
    ShaderResourceManifest &manifest)
{
    GLSLCodeModuleID roots[64]{};
    uint32 root_count = 0;
    for (const GLSLCodeModuleID id : definition.code_module_requirements)
    {
        if (root_count >= 64u)
            return false;
        roots[root_count++] = id;
    }

    return BuildShaderResourceManifest(roots, root_count, manifest);
}

inline std::vector<FixedDescriptorEntry> Build2DDescriptorsFromDefinition(
    const Material2DBuildParams &p,
    const ShaderResourceManifest &manifest)
{
    std::vector<FixedDescriptorEntry> descriptors;
    PushBaseDescriptorEntries(descriptors, p);

    for (uint32 i = 0; i < manifest.ubo_count; ++i)
    {
        const UBODescriptorSemantic semantic = manifest.ubos[i].semantic;
        bool exists = false;
        for (const auto &entry : descriptors)
        {
            UBODescriptorSemantic existing{};
            if (entry.kind == DescriptorKind::UBO
             && TryGetUBODescriptorSemantic(entry.semantic, existing)
             && existing == semantic)
            {
                exists = true;
                break;
            }
        }
        if (exists)
            continue;

        switch (semantic)
        {
        case UBODescriptorSemantic::ViewportInfo:
            descriptor_builder_common::PushViewport(descriptors, manifest.ubos[i].stage_flags);
            break;
        case UBODescriptorSemantic::CameraInfo:
            descriptor_builder_common::PushCamera(descriptors, manifest.ubos[i].stage_flags);
            break;
        case UBODescriptorSemantic::SkyInfo:
            descriptor_builder_common::PushSky(descriptors, manifest.ubos[i].stage_flags);
            break;
        case UBODescriptorSemantic::MaterialColorPalette:
            descriptors.push_back({
                DescriptorSetType::Material, DescriptorKind::UBO, manifest.ubos[i].stage_flags,
                "color_palette", "ColorPalette", nullptr, DescriptorSemantic::MaterialColorPalette,
                TextureSlot::BaseColor, DefaultMaterialDataSlot, SSBOType::UserDefined,
                DescriptorSemanticLayer::UBO
            });
            break;
        }
    }

    for (uint32 i = 0; i < manifest.texture_count; ++i)
    {
        bool exists = false;
        for (const auto &entry : descriptors)
        {
            if (entry.name && manifest.textures[i].name
             && std::strcmp(entry.name, manifest.textures[i].name) == 0)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
            descriptor_builder_common::PushManifestTexture(descriptors, manifest.textures[i]);
    }

    return descriptors;
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
