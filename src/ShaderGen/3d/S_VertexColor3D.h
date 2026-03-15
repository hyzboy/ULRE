#pragma once

#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/common/RenderAssignDef.h>

namespace hgl::graph::mtl {
namespace {

constexpr FixedVertexEntry VERTEX_COLOR_3D_VERTEX[] = {
    { VAT_VEC3, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Position },
    { VAT_VEC4, VertexInputGroup::Basic, VertexInputRate::Vertex, VAN::Color },
    { Assign::TransformID::VAT_FMT, VertexInputGroup::TransformID, VertexInputRate::Instance, Assign::TransformID::VIS_NAME },
};

#ifdef HGL_L2W_USE_SSBO
constexpr DescriptorKind VERTEX_COLOR_3D_L2W_KIND = DescriptorKind::SSBO;
#endif
#ifdef HGL_L2W_USE_UBO
constexpr DescriptorKind VERTEX_COLOR_3D_L2W_KIND = DescriptorKind::UBO;
#endif

constexpr FixedDescriptorEntry VERTEX_COLOR_3D_DESCRIPTORS[] = {
    { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "viewport", "ViewportInfo", nullptr },
    { DescriptorSetType::Scene, DescriptorKind::UBO, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "camera", "CameraInfo", nullptr },
    { DescriptorSetType::Transform, VERTEX_COLOR_3D_L2W_KIND, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS), "l2w", "LocalToWorldData", nullptr },
};

constexpr FixedMaterialDef VERTEX_COLOR_3D_DEF {
    "VertexColor3D",
    PrimitiveType::Triangles,
    VERTEX_COLOR_3D_VERTEX,
    uint32_t(sizeof(VERTEX_COLOR_3D_VERTEX) / sizeof(VERTEX_COLOR_3D_VERTEX[0])),
    VERTEX_COLOR_3D_DESCRIPTORS,
    uint32_t(sizeof(VERTEX_COLOR_3D_DESCRIPTORS) / sizeof(VERTEX_COLOR_3D_DESCRIPTORS[0])),
    nullptr,
    0,
};

}
}


