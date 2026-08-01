#pragma once

/// Build2DCommon.h — 2D 材质转换公共辅助
///
/// 提供 Build2DPreamble、DEF 构建等工具，
/// 供各 M_Xxx2D.cpp 工厂函数使用。
/// GLSL 代码已移至 ShaderLibrary/2d/ 目录下的文件。

#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<string>
#include<vector>

namespace hgl::graph::mtl{

/// 2D 材质内部构建参数——从 MaterialDefinitionBuildRequest/MaterialDefinition 转换而来。
struct Material2DBuildParams
{
    PrimitiveType           prim                = PrimitiveType::Triangles;
    CoordinateSystem2D      coordinate_system   = CoordinateSystem2D::NDC;
    bool                    local_to_world      = true;
    bool                    material_instance   = false;
    uint32_t                shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);
    const GeometryVertexFormat *geometry_vertex_format = nullptr;
    const ShaderBufferSource *const *private_shader_buffer_sources = nullptr;
    uint32_t                private_shader_buffer_source_count = 0;
    const std::vector<MaterialSSBOSlotDecl> *ssbo_slot_decls = nullptr;

    static Material2DBuildParams From(const MaterialDefinitionBuildRequest &request,
                                      const MaterialDefinition &definition)
    {
        Material2DBuildParams p;
        p.prim              = request.primitive_type;
        p.coordinate_system = definition.is_text
            ? definition.coordinate_system_2d
            : request.recipe.coordinate_system_2d;
        p.local_to_world    = definition.is_text
            ? definition.local_to_world_2d
            : request.recipe.local_to_world_2d;
        p.material_instance = false;  // creators set this explicitly when needed
        if(request.override_shader_stage_bits)
            p.shader_stage_flag_bit = request.shader_stage_flag_bit;
        p.geometry_vertex_format            = request.geometry_vertex_format;
        p.private_shader_buffer_sources     = request.private_shader_buffer_sources;
        p.private_shader_buffer_source_count = request.private_shader_buffer_source_count;
        p.ssbo_slot_decls = definition.ssbo_slot_decls.empty() ? nullptr : &definition.ssbo_slot_decls;
        if(request.override_rt_output)
        {
            // rt_output override not stored in Material2DBuildParams (not needed by build helpers)
        }
        return p;
    }
};

namespace build2d{

// ─────────────────────────────────────────────────────────────
// GLSL type string from VkFormat
// ─────────────────────────────────────────────────────────────

inline const char *GLSLInputType(const VkFormat fmt)
{
    switch (fmt)
    {
    case VK_FORMAT_R32G32_SINT:             return "ivec2";
    case VK_FORMAT_R32G32_SFLOAT:           return "vec2";
    case VK_FORMAT_R32G32B32_SFLOAT:        return "vec3";
    case VK_FORMAT_R32G32B32A32_SFLOAT:     return "vec4";
    default:                                return "vec2";
    }
}

// ─────────────────────────────────────────────────────────────
// Descriptor layout macros for fixed descriptor set plan.
// ─────────────────────────────────────────────────────────────

inline std::string BuildDescriptorDefines(
    const Material2DBuildParams &p,
    bool has_texture,
    bool has_mi)
{
    std::string defs;
    const int tex_binding = has_mi ? 3 : 0;

    if(p.coordinate_system == CoordinateSystem2D::Ortho)
    {
        defs += "#define SCENE_SET 0\n";
        defs += "#define VIEWPORT_BINDING 2\n";
    }

    if(p.local_to_world)
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

    if(has_mi)
    {
        defs += "#define MI_SET 2\n";
        defs += "#define MI_BINDING 0\n";
        defs += "#define MI_DATA_INDEX_ROWS_SET 2\n";
        defs += "#define MI_DATA_INDEX_ROWS_BINDING 1\n";
        defs += "#define MI_TEXTURE_LAYER_ROWS_SET 2\n";
        defs += "#define MI_TEXTURE_LAYER_ROWS_BINDING 2\n";
    }

    return defs;
}

// ─────────────────────────────────────────────────────────────
// Shader preamble builder
// ─────────────────────────────────────────────────────────────

inline std::string Build2DPreamble(const Material2DBuildParams &p, bool has_texture, bool has_mi, VkFormat position_format_override = VK_FORMAT_UNDEFINED)
{
    std::string pr = "#version 450\n\n";
    pr += BuildDescriptorDefines(p, has_texture, has_mi);

    const VkFormat position_format = (position_format_override!=VK_FORMAT_UNDEFINED)
                                   ? position_format_override
                                   : ResolveMaterialPositionFormat(p.geometry_vertex_format, VK_FORMAT_R32G32_SFLOAT);

    pr += "#define POSITION_FORMAT ";
    pr += GLSLInputType(position_format);
    pr += "\n";

    switch(p.coordinate_system)
    {
        case CoordinateSystem2D::NDC:       pr += "#define COORD_NDC\n"; break;
        case CoordinateSystem2D::ZeroToOne: pr += "#define COORD_ZEROTOONE\n"; break;
        case CoordinateSystem2D::Ortho:     pr += "#define COORD_ORTHO\n"; break;
    }

    if(p.local_to_world)    pr += "#define HAS_L2W\n";
    if(p.material_instance) pr += "#define HAS_MI\n";

    pr += "\n";
    return pr;
}

// ─────────────────────────────────────────────────────────────
// Common FixedVertexEntry builders
// ─────────────────────────────────────────────────────────────

inline void PushBaseVertexEntries(std::vector<FixedVertexEntry> &v, const Material2DBuildParams &p, VkFormat position_format_override = VK_FORMAT_UNDEFINED)
{
    const VkFormat position_format = (position_format_override!=VK_FORMAT_UNDEFINED)
                                   ? position_format_override
                                   : ResolveMaterialPositionFormat(p.geometry_vertex_format, VK_FORMAT_R32G32_SFLOAT);

    v.push_back({ position_format, VertexSemantic::Position });
}

inline void PushSemanticVertexEntry(std::vector<FixedVertexEntry> &v,
                                    const Material2DBuildParams &p,
                                    const VertexSemantic semantic,
                                    const VkFormat fallback_format)
{
    v.push_back({ ResolveMaterialVertexSemanticFormat(p.geometry_vertex_format, semantic, fallback_format), semantic });
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
    if(p.coordinate_system == CoordinateSystem2D::Ortho)
        v.push_back({DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::UBO});

    // L2W (Transform set) — only if L2W
    if(p.local_to_world)
    {
        v.push_back({DescriptorSetType::Transform, L2W_KIND_2D, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(L2W_KIND_2D)});
        v.push_back({DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO});
    }

    if(p.material_instance)
    {
        v.push_back({DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialSSBOSlotData, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::PBRSurface, GetDescriptorSemanticLayerByKind(MaterialInstanceDescriptorKind)});
        v.push_back({DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialSSBOIndexTable, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO});
        v.push_back({DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable, TextureSlot::BaseColor, DefaultMaterialSSBOSlot, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO});
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
    bc.material_instance               = p.material_instance;
    bc.with_local_to_world             = p.local_to_world;
    bc.with_camera                     = false;
    bc.with_sky                        = false;
    bc.sky_ambient_model               = SkyLightAmbientModel::Simple;
    bc.private_shader_buffer_sources   = p.private_shader_buffer_sources;
    bc.private_shader_buffer_source_count = p.private_shader_buffer_source_count;
    bc.geometry_vertex_format          = p.geometry_vertex_format;
    bc.ssbo_slot_decls                 = p.ssbo_slot_decls;
    return bc;
}

}//namespace build2d
}//namespace hgl::graph::mtl

