#pragma once

/// Build2DCommon.h �?2D 材质转换公共辅助
///
/// 提供 Build2DPreamble、DEF 构建等工具，
/// 供各 M_Xxx2D.cpp 工厂函数使用�?
/// GLSL 代码已移�?ShaderLibrary/2d/ 目录下的文件�?

#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/mtl/DescriptorBindingContract.h>
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
// Shader preamble builder �?#version + #define lines
// C++ only produces the preamble; GLSL code lives in files.
//
//   std::string vs = preamble + "#include \"2d/xxx.vert.glsl\"\n";
//   std::string fs = preamble + "#include \"2d/xxx.frag.glsl\"\n";
// ─────────────────────────────────────────────────────────────

inline std::string Build2DPreamble(const Material2DCreateConfig *cfg,
                                   bool has_texture,
                                   bool has_mi,
                                   const SamplerSlot texture_slot = SamplerSlot::BaseColor,
                                   const bool texture_array_mode = false)
{
    (void)has_texture;
    (void)has_mi;

    std::string p = "#version 450\n\n";

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

    if (texture_array_mode)
    {
        p += "#define TEXTURE_ARRAY_MODE 1\n";

        p += "#define TEX_";
        switch (texture_slot)
        {
            case SamplerSlot::BaseColor: p += "BASECOLOR"; break;
            case SamplerSlot::Normal:    p += "NORMAL";    break;
            case SamplerSlot::Tangent:   p += "TANGENT";   break;
            case SamplerSlot::Metallic:  p += "METALLIC";  break;
            case SamplerSlot::Roughness: p += "ROUGHNESS"; break;
            case SamplerSlot::Height:    p += "HEIGHT";    break;
            case SamplerSlot::Opacity:   p += "OPACITY";   break;
            case SamplerSlot::Text:      p += "TEXT";      break;
            default:                     p += "BASECOLOR"; break;
        }
        p += "_ARRAY 1\n";
    }

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

inline void PushBaseDescriptorEntries(std::vector<FixedDescriptorEntry> &v, const Material2DCreateConfig *cfg)
{
    const bool has_transform_pair = cfg->local_to_world;
    const bool has_material_instance_pair = cfg->material_instance;

    // Viewport (Scene set) �?only for Ortho
    if(cfg->coordinate_system == CoordinateSystem2D::Ortho)
        v.push_back(MakeFixedDescriptorEntry(DescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)));

    // L2W (Transform set) �?only if L2W
    if(has_transform_pair)
        v.push_back(MakeFixedDescriptorEntry(DescriptorSemantic::LocalToWorld, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)));

    if(has_transform_pair)
        v.push_back(MakeFixedDescriptorEntry(DescriptorSemantic::TransformID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT)));

    if(has_material_instance_pair)
        v.push_back(MakeFixedDescriptorEntry(DescriptorSemantic::MaterialInstanceID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT)));

    if(has_material_instance_pair)
        v.push_back(MakeFixedDescriptorEntry(DescriptorSemantic::MaterialInstance, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)));
}

}//namespace build2d
}//namespace hgl::graph::mtl
