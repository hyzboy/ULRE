#pragma once

/// Build2DCommon.h �?2D 材质转换公共辅助
///
/// 提供 Build2DPreamble、DEF 构建等工具，
/// 供各 M_Xxx2D.cpp 工厂函数使用�?
/// GLSL 代码已移�?ShaderLibrary/2d/ 目录下的文件�?

#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/common/RenderAssignDef.h>
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

    if(cfg->coordinate_system == CoordinateSystem2D::Ortho)
    {
        defs += "#define SCENE_SET "       + std::to_string(set) + "\n";
        defs += "#define VIEWPORT_BINDING 0\n";
        set++;
    }

    if(cfg->local_to_world)
    {
        defs += "#define L2W_SET "    + std::to_string(set) + "\n";
        defs += "#define L2W_BINDING 0\n";
        defs += "#define TID_SET "    + std::to_string(set) + "\n";
        defs += "#define TID_BINDING 2\n";
        defs += "#define MID_SET "    + std::to_string(set) + "\n";
        defs += "#define MID_BINDING 1\n";
        set++;
    }

    if(has_texture || has_mi)
    {
        // Resort sorts by name: "Texture..." (T=84) < "mtl" (m=109)
        int binding = 0;
        if(has_texture)
        {
            defs += "#define TEX_SET "     + std::to_string(set) + "\n";
            defs += "#define TEX_BINDING " + std::to_string(binding++) + "\n";
        }
        if(has_mi)
        {
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

    p += "#define TRANSFORM_ID_FROM_DESCRIPTOR 1\n";

    p += "#define MATERIAL_INSTANCE_ID_FROM_DESCRIPTOR 1\n";

    p += "\n";
    return p;
}

// ─────────────────────────────────────────────────────────────
// Common FixedVertexEntry builders
// ─────────────────────────────────────────────────────────────

inline void PushBaseVertexEntries(std::vector<FixedVertexEntry> &v, const Material2DCreateConfig *cfg)
{
    // Position
    v.push_back({cfg->position_format, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position});

    // TransformID (if L2W)
    (void)cfg;

    // MaterialInstanceID is descriptor-backed in SSBO-only mode.
}

// ─────────────────────────────────────────────────────────────
// Common FixedDescriptorEntry builders
// ─────────────────────────────────────────────────────────────

constexpr DescriptorKind L2W_KIND_2D = DescriptorKind::SSBO;

inline void PushBaseDescriptorEntries(std::vector<FixedDescriptorEntry> &v, const Material2DCreateConfig *cfg)
{
    // Viewport (Scene set) �?only for Ortho
    if(cfg->coordinate_system == CoordinateSystem2D::Ortho)
        v.push_back({DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr});

    // L2W (Transform set) �?only if L2W
    if(cfg->local_to_world)
        v.push_back({DescriptorSetType::Transform, L2W_KIND_2D, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr});

    if(cfg->local_to_world)
        v.push_back({DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "tid", "TransformIDData", nullptr});

    if(cfg->local_to_world && cfg->material_instance)
        v.push_back({DescriptorSetType::Transform, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "mid", "MaterialInstanceIDData", nullptr});
}

}//namespace build2d
}//namespace hgl::graph::mtl
