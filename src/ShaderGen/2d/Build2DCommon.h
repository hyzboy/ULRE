#pragma once

/// Build2DCommon.h — 2D 材质转换公共辅助
///
/// 提供 Build2DPreamble、DEF 构建等工具，
/// 供各 M_Xxx2D.cpp 工厂函数使用。
/// GLSL 代码已移至 ShaderLibrary/2d/ 目录下的文件。

#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/common/RenderAssignDef.h>
#include<hgl/mtl/UBOCommon.h>
#include<string>
#include<vector>

namespace hgl::graph::mtl{
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
// Set IDs are now stable (Scene=0, Transform=1, MaterialProgram=2...),
// so we generate matching #define lines for GLSL to reference.
//
// Produced macros (only when the feature is active):
//   SCENE_SET    / VIEWPORT_BINDING  — Scene set (Ortho only)
//   L2W_SET      / L2W_BINDING       — Transform set (L2W only)
//   TEX_SET      / TEX_BINDING        — texture in MaterialProgram set
//   MI_SET       / MI_BINDING         — MI SSBO in MaterialProgram set
// ─────────────────────────────────────────────────────────────

inline std::string BuildDescriptorDefines(
    const Material2DCreateConfig *cfg,
    bool has_texture,
    bool has_mi)
{
    std::string defs;
    const int tex_binding = has_mi ? 3 : 0;

    if(cfg->coordinate_system == CoordinateSystem2D::Ortho)
    {
        defs += "#define SCENE_SET 0\n";
        defs += "#define VIEWPORT_BINDING 2\n";
    }

    if(cfg->local_to_world)
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
// Shader preamble builder — #version + #define lines
// C++ only produces the preamble; GLSL code lives in files.
//
//   std::string vs = preamble + "#include \"2d/xxx.vert.glsl\"\n";
//   std::string fs = preamble + "#include \"2d/xxx.frag.glsl\"\n";
// ─────────────────────────────────────────────────────────────

inline std::string Build2DPreamble(const Material2DCreateConfig *cfg, bool has_texture, bool has_mi, VkFormat position_format_override = VK_FORMAT_UNDEFINED)
{
    std::string p = "#version 450\n\n";
    p += BuildDescriptorDefines(cfg, has_texture, has_mi);

    const VkFormat position_format = (position_format_override!=VK_FORMAT_UNDEFINED)
                                   ? position_format_override
                                   : ResolveMaterialPositionFormat(cfg, VK_FORMAT_R32G32_SFLOAT);

    p += "#define POSITION_FORMAT ";
    p += GLSLInputType(position_format);
    p += "\n";

    switch(cfg->coordinate_system)
    {
        case CoordinateSystem2D::NDC:       p += "#define COORD_NDC\n"; break;
        case CoordinateSystem2D::ZeroToOne: p += "#define COORD_ZEROTOONE\n"; break;
        case CoordinateSystem2D::Ortho:     p += "#define COORD_ORTHO\n"; break;
    }

    if(cfg->local_to_world)     p += "#define HAS_L2W\n";
    if(cfg->material_instance)  p += "#define HAS_MI\n";

    p += "\n";
    return p;
}

// ─────────────────────────────────────────────────────────────
// Common FixedVertexEntry builders
// ─────────────────────────────────────────────────────────────

inline void PushBaseVertexEntries(std::vector<FixedVertexEntry> &v, const Material2DCreateConfig *cfg, VkFormat position_format_override = VK_FORMAT_UNDEFINED)
{
    const VkFormat position_format = (position_format_override!=VK_FORMAT_UNDEFINED)
                                   ? position_format_override
                                   : ResolveMaterialPositionFormat(cfg, VK_FORMAT_R32G32_SFLOAT);

    // Position
    v.push_back({ position_format, VertexSemantic::Position });
}

inline void PushSemanticVertexEntry(std::vector<FixedVertexEntry> &v,
                                    const Material2DCreateConfig *cfg,
                                    const VertexSemantic semantic,
                                    const VkFormat fallback_format)
{
    v.push_back({ ResolveMaterialVertexSemanticFormat(cfg, semantic, fallback_format), semantic });
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

inline void PushBaseDescriptorEntries(std::vector<FixedDescriptorEntry> &v, const Material2DCreateConfig *cfg)
{
    // Viewport (Scene set) — only for Ortho
    if(cfg->coordinate_system == CoordinateSystem2D::Ortho)
        v.push_back({DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr, DescriptorSemantic::ViewportInfo, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::UBO});

    // L2W (Transform set) — only if L2W
    if(cfg->local_to_world)
    {
        v.push_back({DescriptorSetType::Transform, L2W_KIND_2D, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr, DescriptorSemantic::LocalToWorld, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, GetDescriptorSemanticLayerByKind(L2W_KIND_2D)});
        v.push_back({DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w_index_rows", "LocalToWorldIndexRows", nullptr, DescriptorSemantic::LocalToWorldIndexTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO});
    }

    if(cfg->material_instance)
    {
        v.push_back({DescriptorSetType::Material, MaterialInstanceDescriptorKind, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr, DescriptorSemantic::MaterialInstance, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::PBRSurface, GetDescriptorSemanticLayerByKind(MaterialInstanceDescriptorKind)});
        v.push_back({DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_data_index_rows", "DataIndexRows", nullptr, DescriptorSemantic::MaterialDataIndexTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO});
        v.push_back({DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl_texture_layer_rows", "TextureLayerRows", nullptr, DescriptorSemantic::MaterialTextureLayerTable, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::SSBO});
    }
}

}//namespace build2d
}//namespace hgl::graph::mtl
