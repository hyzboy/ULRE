#pragma once

#include <hgl/graph/mtl/FixedMaterialDef.h>
#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/vk/VKRenderAssign.h>

namespace hgl::graph::mtl {
namespace {

constexpr const char VERTEX_LUMINANCE_3D_MI_CODES[] = "vec4 Color;";
constexpr const uint32_t VERTEX_LUMINANCE_3D_MI_BYTES = sizeof(hgl::math::Vector4f);

constexpr FixedVertexEntry VERTEX_LUMINANCE_3D_VERTEX_VEC3[] = {
    { VAT_VEC3, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Position },
    { VAT_FLOAT, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Luminance },
    { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::TransformID::VIS_NAME },
    { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::MaterialInstanceID::VIS_NAME },
};

constexpr FixedVertexEntry VERTEX_LUMINANCE_3D_VERTEX_VEC2[] = {
    { VAT_VEC2, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Position },
    { VAT_FLOAT, VertexInputGroup::Basic, VK_VERTEX_INPUT_RATE_VERTEX, VAN::Luminance },
    { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::TransformID::VIS_NAME },
    { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VK_VERTEX_INPUT_RATE_INSTANCE, Assign::MaterialInstanceID::VIS_NAME },
};

#if defined(HGL_L2W_USE_SSBO) && HGL_L2W_USE_SSBO
constexpr DescriptorKind VERTEX_LUMINANCE_3D_L2W_KIND = DescriptorKind::SSBO;
#else
constexpr DescriptorKind VERTEX_LUMINANCE_3D_L2W_KIND = DescriptorKind::UBO;
#endif

#if defined(HGL_MI_USE_SSBO) && HGL_MI_USE_SSBO
constexpr DescriptorKind VERTEX_LUMINANCE_3D_MI_KIND = DescriptorKind::SSBO;
#else
constexpr DescriptorKind VERTEX_LUMINANCE_3D_MI_KIND = DescriptorKind::UBO;
#endif

constexpr FixedDescriptorEntry VERTEX_LUMINANCE_3D_DESCRIPTORS[] = {
    { DescriptorSetType::RenderTarget, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
    { DescriptorSetType::Camera, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
    { DescriptorSetType::PerFrame, VERTEX_LUMINANCE_3D_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
    { DescriptorSetType::PerMaterial, VERTEX_LUMINANCE_3D_MI_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
};

constexpr const char VERTEX_LUMINANCE_3D_VS_BUSINESS[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    MaterialInstance mi = GetMI();
    Output.Color = vi.Luminance * mi.Color;
    return vec4(vi.Position, 1.0);
}
)";

constexpr const char VERTEX_LUMINANCE_3D_FS_BUSINESS[] = R"(
vec4 FragmentShaderBusiness()
{
    return Input.Color;
}
)";

constexpr VertexShaderBusiness VERTEX_LUMINANCE_3D_VERTEX_BUSINESS { VERTEX_LUMINANCE_3D_VS_BUSINESS };
constexpr FragmentShaderBusiness VERTEX_LUMINANCE_3D_FRAGMENT_BUSINESS { VERTEX_LUMINANCE_3D_FS_BUSINESS };

constexpr FixedMaterialDef VERTEX_LUMINANCE_3D_DEF_VEC3 {
    "VertexLuminance3D",
    PrimitiveType::Triangles,
    VERTEX_LUMINANCE_3D_VERTEX_VEC3,
    uint32_t(sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC3) / sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC3[0])),
    VERTEX_LUMINANCE_3D_DESCRIPTORS,
    uint32_t(sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS) / sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS[0])),
    VERTEX_LUMINANCE_3D_MI_CODES,
    VERTEX_LUMINANCE_3D_MI_BYTES,
    nullptr,
    nullptr,
    nullptr,
};

constexpr FixedMaterialDef VERTEX_LUMINANCE_3D_DEF_VEC2 {
    "VertexLuminance3D",
    PrimitiveType::Triangles,
    VERTEX_LUMINANCE_3D_VERTEX_VEC2,
    uint32_t(sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC2) / sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC2[0])),
    VERTEX_LUMINANCE_3D_DESCRIPTORS,
    uint32_t(sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS) / sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS[0])),
    VERTEX_LUMINANCE_3D_MI_CODES,
    VERTEX_LUMINANCE_3D_MI_BYTES,
    nullptr,
    nullptr,
    nullptr,
};

const ComposedMaterialDef VERTEX_LUMINANCE_3D_COMPOSED_DEF_VEC3 {
    "VertexLuminance3D",
    PrimitiveType::Triangles,
    VERTEX_LUMINANCE_3D_VERTEX_VEC3,
    uint32_t(sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC3) / sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC3[0])),
    VERTEX_LUMINANCE_3D_DESCRIPTORS,
    uint32_t(sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS) / sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS[0])),
    &VERTEX_LUMINANCE_3D_VERTEX_BUSINESS,
    &VERTEX_LUMINANCE_3D_FRAGMENT_BUSINESS,
    ShaderOutputMode::SingleRTAlphaBlend,
    false,
    VERTEX_LUMINANCE_3D_MI_CODES,
    VERTEX_LUMINANCE_3D_MI_BYTES,
};

const ComposedMaterialDef VERTEX_LUMINANCE_3D_COMPOSED_DEF_VEC2 {
    "VertexLuminance3D",
    PrimitiveType::Triangles,
    VERTEX_LUMINANCE_3D_VERTEX_VEC2,
    uint32_t(sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC2) / sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC2[0])),
    VERTEX_LUMINANCE_3D_DESCRIPTORS,
    uint32_t(sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS) / sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS[0])),
    &VERTEX_LUMINANCE_3D_VERTEX_BUSINESS,
    &VERTEX_LUMINANCE_3D_FRAGMENT_BUSINESS,
    ShaderOutputMode::SingleRTAlphaBlend,
    false,
    VERTEX_LUMINANCE_3D_MI_CODES,
    VERTEX_LUMINANCE_3D_MI_BYTES,
};

}
}
