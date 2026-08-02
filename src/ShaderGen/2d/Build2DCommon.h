#pragma once

/// Build2DCommon.h — 2D 材质转换公共辅助
///
/// 提供 2D FS preamble、DEF 构建等工具，
/// 供各 M_Xxx2D.cpp 工厂函数使用。
/// GLSL 代码已移至 ShaderLibrary/2d/ 目录下的文件。

#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/mtl/UBOCommon.h>
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
    const ShaderBufferSource *const *private_shader_buffer_sources = nullptr;
    uint32_t                private_shader_buffer_source_count = 0;
    const std::vector<MaterialSSBOSlotDecl> *ssbo_slot_decls = nullptr;

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
        p.private_shader_buffer_sources     = request.private_shader_buffer_sources;
        p.private_shader_buffer_source_count = request.private_shader_buffer_source_count;
        p.ssbo_slot_decls = definition.ssbo_slot_decls.empty() ? nullptr : &definition.ssbo_slot_decls;
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
    std::string defs = "#version 450\n\n";
    const bool has_ssbo = p.material_definition && !p.material_definition->ssbo_slot_decls.empty();
    const int tex_binding = has_ssbo ? 3 : 0;

    if(p.vertex_node_config.projection == ProjectionMode::OrthoViewport
    || p.vertex_node_config.projection == ProjectionMode::OrthoThenLocalToWorld)
    {
        defs += "#define SCENE_SET 0\n";
        defs += "#define VIEWPORT_BINDING 2\n";
    }

    if(p.vertex_node_config.projection == ProjectionMode::LocalToWorldOnly
    || p.vertex_node_config.projection == ProjectionMode::OrthoThenLocalToWorld
    || p.vertex_node_config.projection == ProjectionMode::WorldCameraVP)
    {
        defs += "#define L2W_SET 1\n";
        defs += "#define L2W_BINDING 0\n";
        defs += "#define L2W_INDEX_ROWS_SET 1\n";
        defs += "#define L2W_INDEX_ROWS_BINDING 1\n";
    }

    if(has_texture)
    {
        defs += "#define TEX_SET 2\n";
        defs += "#define TEX_BINDING " + std::to_string(tex_binding) + "\n";
    }

    if(has_ssbo)
    {
        defs += "#define MI_SET 2\n";
        defs += "#define MI_BINDING 0\n";
        defs += "#define MI_DATA_INDEX_ROWS_SET 2\n";
        defs += "#define MI_DATA_INDEX_ROWS_BINDING 1\n";
        defs += "#define MI_TEXTURE_LAYER_ROWS_SET 2\n";
        defs += "#define MI_TEXTURE_LAYER_ROWS_BINDING 2\n";
    }

    defs += "\n";
    return defs;
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

#ifdef HGL_L2W_USE_SSBO
    constexpr DescriptorKind L2W_KIND_2D = DescriptorKind::SSBO;
#endif
#ifdef HGL_L2W_USE_UBO
    constexpr DescriptorKind L2W_KIND_2D = DescriptorKind::UBO;
#endif

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
        descriptor_builder_common::PushLocalToWorld(v, L2W_KIND_2D, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
        descriptor_builder_common::PushLocalToWorldIndexRows(v, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
    }

    // MaterialInstance — definition-driven (E4): driven by ssbo_slot_decls, same as 3D path.
    if (p.material_definition && !p.material_definition->ssbo_slot_decls.empty())
    {
        const SSBOType ssbo_type = p.material_definition->ssbo_slot_decls.front().ssbo_type;
        descriptor_builder_common::PushMaterialInstance(v, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), ssbo_type);
        descriptor_builder_common::PushMaterialDataIndexRows(v, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
        descriptor_builder_common::PushMaterialTextureLayerRows(v, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
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

// ─────────────────────────────────────────────────────────────
// Convert Material2DBuildParams → CompositorMaterialBuildConfig (2D: no camera/sky)
// ─────────────────────────────────────────────────────────────

inline CompositorMaterialBuildConfig ToCompositorBuildConfig2D(const Material2DBuildParams &p)
{
    CompositorMaterialBuildConfig bc;
    bc.primitive_type                  = p.prim;
    bc.shader_stage_flag_bits          = p.shader_stage_flag_bit;
    bc.sky_ambient_model               = SkyLightAmbientModel::Simple;
    bc.private_shader_buffer_sources   = p.private_shader_buffer_sources;
    bc.private_shader_buffer_source_count = p.private_shader_buffer_source_count;
    bc.geometry_vertex_format          = p.geometry_vertex_format;
    bc.ssbo_slot_decls                 = p.ssbo_slot_decls;
    bc.material_definition             = p.material_definition;
    return bc;
}

}//namespace build2d
}//namespace hgl::graph::mtl
