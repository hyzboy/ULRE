#pragma once

#include <hgl/common/RenderAssignDef.h>

namespace hgl::graph::mtl {
namespace {

// ─────────────────────────────────────────────────────────────────────────────
// 顶点输入和描述符定义
// ─────────────────────────────────────────────────────────────────────────────

constexpr FixedVertexEntry GIZMO_3D_VERTEX[] = {
    { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
    { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Normal },
    { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VertexInputRate::Instance, Assign::TransformID::VIS_NAME },
    { Assign::MaterialInstanceID::VAT_FMT, VertexInputGroup::MaterialInstanceID, VertexInputRate::Instance, Assign::MaterialInstanceID::VIS_NAME },
};

#ifdef HGL_L2W_USE_SSBO
constexpr DescriptorKind GIZMO_3D_L2W_KIND = DescriptorKind::SSBO;
#endif
#ifdef HGL_L2W_USE_UBO
constexpr DescriptorKind GIZMO_3D_L2W_KIND = DescriptorKind::UBO;
#endif

constexpr FixedDescriptorEntry GIZMO_3D_DESCRIPTORS[] = {
    { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
    { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
    { DescriptorSetType::Transform, GIZMO_3D_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
    { DescriptorSetType::Material, DescriptorKind::SSBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "mtl", "MaterialInstanceData", nullptr },
};

// ─────────────────────────────────────────────────────────────────────────────
// 材质实例定义
// ─────────────────────────────────────────────────────────────────────────────

constexpr const char GIZMO_3D_MI_GLSL[] = "vec4 Color;";
constexpr uint32_t GIZMO_3D_MI_BYTES = sizeof(math::Vector4f);

constexpr FixedMaterialDef GIZMO_3D_DEF {
    "Gizmo3D",
    PrimitiveType::Triangles,
    GIZMO_3D_VERTEX,
    uint32_t(sizeof(GIZMO_3D_VERTEX) / sizeof(GIZMO_3D_VERTEX[0])),
    GIZMO_3D_DESCRIPTORS,
    uint32_t(sizeof(GIZMO_3D_DESCRIPTORS) / sizeof(GIZMO_3D_DESCRIPTORS[0])),
    GIZMO_3D_MI_GLSL,
    GIZMO_3D_MI_BYTES,
};

}
}


