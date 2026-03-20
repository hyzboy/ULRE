#pragma once

/// Build2DCommon.h �?2D 材质转换公共辅助
///
/// 提供 Build2DPreamble、DEF 构建等工具，
/// 供各 M_Xxx2D.cpp 工厂函数使用�?
/// GLSL 代码已移�?ShaderLibrary/2d/ 目录下的文件�?

#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/mtl/UBOCommon.h>
#include<string>
#include<vector>

namespace hgl::graph::mtl{
namespace build2d{

// ─────────────────────────────────────────────────────────────
// GLSL type string from VAType
// ─────────────────────────────────────────────────────────────

inline const char *GLSLInputType(const VAType &vat)
{
    if(vat==VAT_IVEC2) return "ivec2";
    if(vat==VAT_VEC2)  return "vec2";
    if(vat==VAT_VEC3)  return "vec3";
    if(vat==VAT_VEC4)  return "vec4";
    return "vec2";
}

// ─────────────────────────────────────────────────────────────
// Descriptor layout macros �?Resort() compacts away empty sets,
// so we generate #define lines for GLSL to reference.
//
// Produced macros (only when the feature is active):
//   SCENE_SET    / VIEWPORT_BINDING  �?Scene set (Ortho only)
//   L2W_SET      / L2W_BINDING       �?Transform set (L2W only)
//   TEX_SET      / TEX_BINDING        �?texture in Material set
//   MI_SET       / MI_BINDING         �?MI SSBO in Material set
// ─────────────────────────────────────────────────────────────

inline std::string BuildDescriptorDefines(
    const Material2DCreateConfig *cfg,
    bool has_texture,
    bool has_mi)
{
    std::string defs;
    int set = 0;
    const bool has_transform_pair = cfg->local_to_world;
    const bool has_material_instance_pair = has_mi;

    if(cfg->coordinate_system == CoordinateSystem2D::Ortho)
    {
        defs += "#define SCENE_SET "       + std::to_string(set) + "\n";
        defs += "#define VIEWPORT_BINDING 0\n";
        set++;
    }

    if(has_transform_pair)
    {
        int transform_binding = 0;

        if(has_transform_pair)
        {
            defs += "#define L2W_SET "    + std::to_string(set) + "\n";
            defs += "#define L2W_BINDING " + std::to_string(transform_binding++) + "\n";
        }

        if(has_transform_pair)
        {
            defs += "#define TID_SET "    + std::to_string(set) + "\n";
            defs += "#define TID_BINDING " + std::to_string(transform_binding++) + "\n";
        }

        set++;
    }

    if(has_texture || has_material_instance_pair)
    {
        // Resort sorts by name: "Texture..." (T=84) < "mid" (m=109, i) < "mtl" (m=109, t)
        int binding = 0;
        if(has_texture)
        {
            defs += "#define TEX_SET "     + std::to_string(set) + "\n";
            defs += "#define TEX_BINDING " + std::to_string(binding++) + "\n";
        }
        if(has_material_instance_pair)
        {
            defs += "#define MID_SET "     + std::to_string(set) + "\n";
            defs += "#define MID_BINDING " + std::to_string(binding++) + "\n";
            defs += "#define MI_SET "      + std::to_string(set) + "\n";
            defs += "#define MI_BINDING "  + std::to_string(binding++) + "\n";
        }
        set++;
    }

    return defs;
}

// ─────────────────────────────────────────────────────────────
// Shader preamble builder �?#version + #define lines
// C++ only produces the preamble; GLSL code lives in files.
//
//   std::string vs = preamble + "#include \"2d/xxx.vert.glsl\"\n";
//   std::string fs = preamble + "#include \"2d/xxx.frag.glsl\"\n";
// ─────────────────────────────────────────────────────────────

inline std::string Build2DPreamble(const Material2DCreateConfig *cfg, bool has_texture, bool has_mi)
{
    std::string p = "#version 450\n\n";
    p += BuildDescriptorDefines(cfg, has_texture, has_mi);

    p += "#define POSITION_FORMAT ";
    p += GLSLInputType(cfg->position_format);
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

inline void PushBaseVertexEntries(std::vector<FixedVertexEntry> &v, const Material2DCreateConfig *cfg)
{
    // Position
    v.push_back({cfg->position_format, VertexInputRate::Vertex, VAN::Position});

    // MaterialInstanceID is descriptor-backed in SSBO-only mode.
}

// ─────────────────────────────────────────────────────────────
// Common FixedDescriptorEntry builders
// ─────────────────────────────────────────────────────────────

constexpr DescriptorKind L2W_KIND_2D = DescriptorKind::SSBO;

inline void PushBaseDescriptorEntries(std::vector<FixedDescriptorEntry> &v, const Material2DCreateConfig *cfg)
{
    const bool has_transform_pair = cfg->local_to_world;
    const bool has_material_instance_pair = cfg->material_instance;

    // Viewport (Scene set) �?only for Ortho
    if(cfg->coordinate_system == CoordinateSystem2D::Ortho)
        v.push_back({DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr});

    // L2W (Transform set) �?only if L2W
    if(has_transform_pair)
        v.push_back({DescriptorSetType::Transform, L2W_KIND_2D, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr});

    if(has_transform_pair)
        v.push_back({DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "tid", "TransformIDData", nullptr});

    if(has_material_instance_pair)
        v.push_back({DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "mid", "MaterialInstanceIDData", nullptr});

    if(has_material_instance_pair)
        v.push_back({DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr});
}

}//namespace build2d
}//namespace hgl::graph::mtl
