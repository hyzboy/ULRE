#include"FixedDefFactory3D.h"
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/math/Vector.h>

namespace hgl::graph::mtl
{
namespace
{
    constexpr FixedVertexEntry GIZMO_3D_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Normal },
    };

    const FixedUBODescriptors GIZMO_3D_UBOS = {
        {UBODescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::CameraInfo,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    const FixedSSBODescriptors GIZMO_3D_SSBOS = {
        {SSBODescriptorSemantic::LocalToWorld,       uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {SSBODescriptorSemantic::TransformID,        uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstanceID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstance,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    constexpr const char GIZMO_3D_MI_GLSL[] = "vec4 Color;";
    constexpr uint32_t GIZMO_3D_MI_BYTES = sizeof(math::Vector4f);

    const FixedMaterialDef GIZMO_3D_DEF {
        "Gizmo3D",
        PrimitiveType::Triangles,
        GIZMO_3D_VERTEX,
        uint32_t(sizeof(GIZMO_3D_VERTEX) / sizeof(GIZMO_3D_VERTEX[0])),
        &GIZMO_3D_UBOS,
        &GIZMO_3D_SSBOS,
        nullptr,
        GIZMO_3D_MI_GLSL,
        GIZMO_3D_MI_BYTES,
    };
}

MaterialCreateInfo *CreateGizmo3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    if(cfg)
        cfg->material_instance=true;

    MaterialVariantKey var_key;
    var_key.SetDebugShading(true);
    return CreateFromFixedDef3D("Gizmo3D", profile, GIZMO_3D_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl