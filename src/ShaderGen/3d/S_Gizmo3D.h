#pragma once

#include <hgl/mtl/FixedMaterialDef.h>
#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/shadergen/ShaderLogic.h>
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

#if defined(HGL_L2W_USE_SSBO) && HGL_L2W_USE_SSBO
constexpr DescriptorKind GIZMO_3D_L2W_KIND = DescriptorKind::SSBO;
#else
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

// ─────────────────────────────────────────────────────────────────────────────
// Business 函数（新路径）
// ─────────────────────────────────────────────────────────────────────────────

constexpr const char GIZMO_3D_VS_BUSINESS[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.Normal = GetNormal(vi.Normal);
    Output.Position = GetLocalToWorld() * vec4(vi.Position, 1.0);
    return vec4(vi.Position, 1.0);
}
)";

constexpr const char GIZMO_3D_FS_BUSINESS[] = R"(
vec4 FragmentShaderBusiness()
{
    MaterialInstance mi = GetMI();
    
    const vec3 SUN_DIRECTION = vec3(0.655386, 0.491539, 0.573462);
    const vec3 SUN_COLOR = vec3(1.0, 1.0, 1.0);
    
    float intensity = 0.5 * max(dot(Input.Normal, SUN_DIRECTION), 0.0) + 0.5;
    vec3 direct_color = intensity * SUN_COLOR * mi.Color.rgb;
    
    vec3 spec_color = vec3(0);
    if (intensity > 0.0)
    {
        vec3 half_vector = normalize(SUN_DIRECTION + normalize(Input.Position.xyz + camera.pos));
        float specular = max(dot(half_vector, Input.Normal), 0.0);
        spec_color = specular * pow(specular, 16) * SUN_COLOR;
    }
    
    return vec4(direct_color + spec_color, 1.0);
}
)";

constexpr VertexShaderBusiness GIZMO_3D_VERTEX_BUSINESS { GIZMO_3D_VS_BUSINESS };
constexpr FragmentShaderBusiness GIZMO_3D_FRAGMENT_BUSINESS { GIZMO_3D_FS_BUSINESS };

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

// ─────────────────────────────────────────────────────────────────────────────
// ComposedMaterialDef（新路径）
// ─────────────────────────────────────────────────────────────────────────────

const ComposedMaterialDef GIZMO_3D_COMPOSED_DEF {
    "Gizmo3D",
    PrimitiveType::Triangles,
    GIZMO_3D_VERTEX,
    uint32_t(sizeof(GIZMO_3D_VERTEX) / sizeof(GIZMO_3D_VERTEX[0])),
    GIZMO_3D_DESCRIPTORS,
    uint32_t(sizeof(GIZMO_3D_DESCRIPTORS) / sizeof(GIZMO_3D_DESCRIPTORS[0])),
    &GIZMO_3D_VERTEX_BUSINESS,
    &GIZMO_3D_FRAGMENT_BUSINESS,
    ShaderOutputMode::SingleRTAlphaBlend,
    false,
    GIZMO_3D_MI_GLSL,
    GIZMO_3D_MI_BYTES,
};

// ─────────────────────────────────────────────────────────────────────────────
// Logic 定义（简化版，匹配当前 business 规范）
// ─────────────────────────────────────────────────────────────────────────────

constexpr const char* GIZMO_3D_VERTEX_RESOURCES[] = {
    "l2w",
    "camera"
};

constexpr const char* GIZMO_3D_VERTEX_HELPERS[] = {
    "GetNormal",
    "GetLocalToWorld"
};

constexpr const char* GIZMO_3D_FRAGMENT_RESOURCES[] = {
    "MaterialInstanceData",
    "camera"
};

constexpr const char* GIZMO_3D_FRAGMENT_HELPERS[] = {
    "GetMI"
};

const VertexShaderLogic GIZMO_3D_VERTEX_SHADER_LOGIC = {
    {
        GIZMO_3D_VS_BUSINESS,
        nullptr,
        GIZMO_3D_VERTEX_RESOURCES,
        2,
        GIZMO_3D_VERTEX_HELPERS,
        2
    }
};

const FragmentShaderLogic GIZMO_3D_FRAGMENT_SHADER_LOGIC = {
    {
        GIZMO_3D_FS_BUSINESS,
        nullptr,
        GIZMO_3D_FRAGMENT_RESOURCES,
        2,
        GIZMO_3D_FRAGMENT_HELPERS,
        1
    }
};

const MaterialLogicDef GIZMO_3D_LOGIC = {
    GIZMO_3D_VERTEX_SHADER_LOGIC,
    GIZMO_3D_FRAGMENT_SHADER_LOGIC,
    nullptr,
    nullptr,
    nullptr
};

}
}


