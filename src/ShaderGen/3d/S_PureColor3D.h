#pragma once

#include <hgl/graph/mtl/FixedMaterialDef.h>
#include <hgl/vk/VKRenderAssign.h>

namespace hgl::graph::mtl {
namespace {

constexpr const char pure_color_3d_mi_codes[] = "vec4 Color;";
constexpr const uint32_t pure_color_3d_mi_bytes = 16;

constexpr FixedVertexEntry PURE_COLOR_3D_VERTEX[] = {
    { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Position },
    { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::TransformID::VIS_NAME },
    { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::MaterialInstanceID::VIS_NAME },
};

#if defined(HGL_L2W_USE_SSBO) && HGL_L2W_USE_SSBO
constexpr DescriptorKind PURE_COLOR_3D_L2W_KIND = DescriptorKind::SSBO;
#else
constexpr DescriptorKind PURE_COLOR_3D_L2W_KIND = DescriptorKind::UBO;
#endif

#if defined(HGL_MI_USE_SSBO) && HGL_MI_USE_SSBO
constexpr DescriptorKind PURE_COLOR_3D_MI_KIND = DescriptorKind::SSBO;
#else
constexpr DescriptorKind PURE_COLOR_3D_MI_KIND = DescriptorKind::UBO;
#endif

constexpr FixedDescriptorEntry PURE_COLOR_3D_DESCRIPTORS[] = {
    { DescriptorSetType::RenderTarget, DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
    { DescriptorSetType::Camera,       DescriptorKind::UBO,  uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera",   "CameraInfo",   nullptr },
    { DescriptorSetType::PerFrame,     PURE_COLOR_3D_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
    { DescriptorSetType::PerMaterial,  PURE_COLOR_3D_MI_KIND,  uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "mtl", "MaterialInstanceData", nullptr },
};

constexpr const char PURE_COLOR_3D_VERT_GLSL[] = R"(
void main()
{
    MaterialInstance mi=GetMI();

    Output.Color=mi.Color;

    gl_Position=GetPosition3D();
})";

constexpr const char PURE_COLOR_3D_FRAG_GLSL[] = R"(
void main()
{
    FragColor=Input.Color;
})";

constexpr FixedMaterialDef PURE_COLOR_3D_DEF {
    "PureColor3D",
    PrimitiveType::Triangles,
    PURE_COLOR_3D_VERTEX,
    uint32_t(sizeof(PURE_COLOR_3D_VERTEX) / sizeof(PURE_COLOR_3D_VERTEX[0])),
    PURE_COLOR_3D_DESCRIPTORS,
    uint32_t(sizeof(PURE_COLOR_3D_DESCRIPTORS) / sizeof(PURE_COLOR_3D_DESCRIPTORS[0])),
    pure_color_3d_mi_codes,
    pure_color_3d_mi_bytes,
    PURE_COLOR_3D_VERT_GLSL,
    nullptr,
    PURE_COLOR_3D_FRAG_GLSL,
};

}
}