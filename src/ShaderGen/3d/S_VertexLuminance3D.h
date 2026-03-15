#pragma once

#include <hgl/common/RenderAssignDef.h>

namespace hgl::graph::mtl {
namespace {

constexpr const char VERTEX_LUMINANCE_3D_MI_CODES[] = "vec4 Color;";
constexpr const uint32_t VERTEX_LUMINANCE_3D_MI_BYTES = sizeof(hgl::math::Vector4f);

constexpr FixedVertexEntry VERTEX_LUMINANCE_3D_VERTEX_VEC3[] = {
    { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
    { VAT_FLOAT, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Luminance },
    { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VertexInputRate::Instance, Assign::TransformID::VIS_NAME },
    { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VertexInputRate::Instance, Assign::MaterialInstanceID::VIS_NAME },
};

constexpr FixedVertexEntry VERTEX_LUMINANCE_3D_VERTEX_VEC2[] = {
    { VAT_VEC2, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
    { VAT_FLOAT, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Luminance },
    { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VertexInputRate::Instance, Assign::TransformID::VIS_NAME },
    { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VertexInputRate::Instance, Assign::MaterialInstanceID::VIS_NAME },
};

#ifdef HGL_L2W_USE_SSBO
constexpr DescriptorKind VERTEX_LUMINANCE_3D_L2W_KIND = DescriptorKind::SSBO;
#endif
#ifdef HGL_L2W_USE_UBO
constexpr DescriptorKind VERTEX_LUMINANCE_3D_L2W_KIND = DescriptorKind::UBO;
#endif

#ifdef HGL_MI_USE_SSBO
constexpr DescriptorKind VERTEX_LUMINANCE_3D_MI_KIND = DescriptorKind::SSBO;
#endif
#ifdef HGL_MI_USE_UBO
constexpr DescriptorKind VERTEX_LUMINANCE_3D_MI_KIND = DescriptorKind::UBO;
#endif

constexpr FixedDescriptorEntry VERTEX_LUMINANCE_3D_DESCRIPTORS[] = {
    { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
    { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
    { DescriptorSetType::Transform, VERTEX_LUMINANCE_3D_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
    { DescriptorSetType::Material, VERTEX_LUMINANCE_3D_MI_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
};

constexpr FixedMaterialDef VERTEX_LUMINANCE_3D_DEF_VEC3 {
    "VertexLuminance3D",
    PrimitiveType::Triangles,
    VERTEX_LUMINANCE_3D_VERTEX_VEC3,
    uint32_t(sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC3) / sizeof(VERTEX_LUMINANCE_3D_VERTEX_VEC3[0])),
    VERTEX_LUMINANCE_3D_DESCRIPTORS,
    uint32_t(sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS) / sizeof(VERTEX_LUMINANCE_3D_DESCRIPTORS[0])),
    VERTEX_LUMINANCE_3D_MI_CODES,
    VERTEX_LUMINANCE_3D_MI_BYTES,
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
};

}
}


