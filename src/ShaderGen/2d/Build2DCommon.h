#pragma once

/// Build2DCommon.h �?2D 材质转换公共辅助
///
/// 提供 Build2DPreamble、DEF 构建等工具，
/// 供各 M_Xxx2D.cpp 工厂函数使用�?
/// GLSL 代码已移�?ShaderLibrary/2d/ 目录下的文件�?

#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/FixedMaterialDef.h>
#include<hgl/mtl/DescriptorBindingContract.h>
#include<hgl/mtl/SamplerName.h>
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

        const size_t slot_index = size_t(texture_slot);
        const char *slot_name = "BaseColor";

        if (slot_index < SamplerSlotCount)
            slot_name = SamplerSlotNameList[slot_index];

        p += ToUpperASCII(slot_name);
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
// Common fixed descriptor builders
// ─────────────────────────────────────────────────────────────

inline void PushBaseUBODescriptors(FixedUBODescriptors &descriptors, const Material2DCreateConfig *cfg)
{
    if(cfg->coordinate_system == CoordinateSystem2D::Ortho)
        AddFixedUBODescriptor(descriptors, UBODescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
}

inline void PushBaseSSBODescriptors(FixedSSBODescriptors &descriptors, const Material2DCreateConfig *cfg)
{
    if(cfg->local_to_world)
    {
        AddFixedSSBODescriptor(descriptors, SSBODescriptorSemantic::LocalToWorld, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
        AddFixedSSBODescriptor(descriptors, SSBODescriptorSemantic::TransformID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT));
    }

    if(cfg->material_instance)
    {
        AddFixedSSBODescriptor(descriptors, SSBODescriptorSemantic::MaterialInstanceID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT));
        AddFixedSSBODescriptor(descriptors, SSBODescriptorSemantic::MaterialInstance, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS));
    }
}

}//namespace build2d
}//namespace hgl::graph::mtl
