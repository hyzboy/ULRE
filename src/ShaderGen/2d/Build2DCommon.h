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

// ─────────────────────────────────────────────────────────────
// Emit GetPosition2D() — direct variant, no macro branching
// ─────────────────────────────────────────────────────────────

inline std::string EmitGetPosition2DGLSL(const Material2DCreateConfig *cfg)
{
    std::string s;

    if(cfg->coordinate_system == CoordinateSystem2D::Ortho)
        s += "#include \"common/ubo_viewport.glsl\"\n";

    if(cfg->local_to_world)
        s += "#include \"common/ssbo_transform.glsl\"\n";

    s += "layout(location=POSITION_LOCATION) in ";
    s += GLSLInputType(cfg->position_format);
    s += " Position;\n\n";

    s += "vec4 GetPosition2D()\n{\n";

    if(cfg->coordinate_system == CoordinateSystem2D::Ortho && cfg->local_to_world)
        s += "    return GetTransform() * viewport.ortho_matrix * vec4(vec2(Position), 0, 1);\n";
    else if(cfg->coordinate_system == CoordinateSystem2D::Ortho)
        s += "    return viewport.ortho_matrix * vec4(vec2(Position), 0, 1);\n";
    else if(cfg->coordinate_system == CoordinateSystem2D::ZeroToOne && cfg->local_to_world)
        s += "    return GetTransform() * vec4(vec2(Position) * 2.0 - 1.0, 0, 1);\n";
    else if(cfg->coordinate_system == CoordinateSystem2D::ZeroToOne)
        s += "    return vec4(vec2(Position) * 2.0 - 1.0, 0, 1);\n";
    else if(cfg->local_to_world)
        s += "    return GetTransform() * vec4(vec2(Position), 0, 1);\n";
    else
        s += "    return vec4(vec2(Position), 0, 1);\n";

    s += "}\n\n";
    return s;
}

// ─────────────────────────────────────────────────────────────
// Shader preamble builder
// ─────────────────────────────────────────────────────────────

inline std::string Build2DFragmentPreamble(const Material2DCreateConfig *cfg,
                                           bool has_texture,
                                           bool has_mi,
                                           const SamplerSlot texture_slot = SamplerSlot::BaseColor,
                                           const bool texture_array_mode = false)
{
    (void)has_texture;
    (void)has_mi;
    (void)texture_slot;
    (void)texture_array_mode;

    std::string p = "#version 450\n\n";

    if(cfg->material_instance)  p += "#define HAS_MI\n";

    p += "\n";

    return p;
}

inline std::string Build2DVertexPreamble(const Material2DCreateConfig *cfg,
                                         bool has_texture,
                                         bool has_mi,
                                         const SamplerSlot texture_slot = SamplerSlot::BaseColor,
                                         const bool texture_array_mode = false)
{
    std::string p = Build2DFragmentPreamble(cfg,
                                            has_texture,
                                            has_mi,
                                            texture_slot,
                                            texture_array_mode);

    p += EmitGetPosition2DGLSL(cfg);

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
