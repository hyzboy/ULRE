#pragma once

#include <hgl/graph/mtl/FixedMaterialDef.h>
#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/vk/VKRenderAssign.h>

namespace hgl::graph::mtl {
namespace {

constexpr FixedVertexEntry VERTEX_PATTLE_COLOR_3D_VERTEX[] = {
    { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Position },
    { VAT_UINT, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Color },
    { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::TransformID::VIS_NAME },
};

#if defined(HGL_L2W_USE_SSBO) && HGL_L2W_USE_SSBO
constexpr DescriptorKind VERTEX_PATTLE_COLOR_3D_L2W_KIND = DescriptorKind::SSBO;
#else
constexpr DescriptorKind VERTEX_PATTLE_COLOR_3D_L2W_KIND = DescriptorKind::UBO;
#endif

constexpr FixedDescriptorEntry VERTEX_PATTLE_COLOR_3D_DESCRIPTORS[] = {
    { DescriptorSetType::RenderTarget, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
    { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
    { DescriptorSetType::PerFrame, VERTEX_PATTLE_COLOR_3D_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
    { DescriptorSetType::PerMaterial, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_VERTEX_BIT), "color_pattle", "ColorPattle", nullptr },
};

constexpr const char VERTEX_PATTLE_COLOR_3D_VS_BUSINESS[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.Color = color_pattle.color[vi.Color];
    return vec4(vi.Position, 1.0);
}
)";

constexpr const char VERTEX_PATTLE_COLOR_3D_FS_BUSINESS[] = R"(
vec4 FragmentShaderBusiness()
{
    return Input.Color;
}
)";

constexpr VertexShaderBusiness VERTEX_PATTLE_COLOR_3D_VERTEX_BUSINESS { VERTEX_PATTLE_COLOR_3D_VS_BUSINESS };
constexpr FragmentShaderBusiness VERTEX_PATTLE_COLOR_3D_FRAGMENT_BUSINESS { VERTEX_PATTLE_COLOR_3D_FS_BUSINESS };

constexpr FixedMaterialDef VERTEX_PATTLE_COLOR_3D_DEF {
    "VertexPattleColor3D",
    PrimitiveType::Triangles,
    VERTEX_PATTLE_COLOR_3D_VERTEX,
    uint32_t(sizeof(VERTEX_PATTLE_COLOR_3D_VERTEX) / sizeof(VERTEX_PATTLE_COLOR_3D_VERTEX[0])),
    VERTEX_PATTLE_COLOR_3D_DESCRIPTORS,
    uint32_t(sizeof(VERTEX_PATTLE_COLOR_3D_DESCRIPTORS) / sizeof(VERTEX_PATTLE_COLOR_3D_DESCRIPTORS[0])),
    nullptr,
    0,
    nullptr,
    nullptr,
    nullptr,
};

const ComposedMaterialDef VERTEX_PATTLE_COLOR_3D_COMPOSED_DEF {
    "VertexPattleColor3D",
    PrimitiveType::Triangles,
    VERTEX_PATTLE_COLOR_3D_VERTEX,
    uint32_t(sizeof(VERTEX_PATTLE_COLOR_3D_VERTEX) / sizeof(VERTEX_PATTLE_COLOR_3D_VERTEX[0])),
    VERTEX_PATTLE_COLOR_3D_DESCRIPTORS,
    uint32_t(sizeof(VERTEX_PATTLE_COLOR_3D_DESCRIPTORS) / sizeof(VERTEX_PATTLE_COLOR_3D_DESCRIPTORS[0])),
    &VERTEX_PATTLE_COLOR_3D_VERTEX_BUSINESS,
    &VERTEX_PATTLE_COLOR_3D_FRAGMENT_BUSINESS,
    ShaderOutputMode::SingleRTAlphaBlend,
    false,
    nullptr,
    0,
};

}
}
