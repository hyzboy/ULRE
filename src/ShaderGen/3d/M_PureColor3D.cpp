#include"FixedDefFactory3D.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char pure_color_3d_mi_codes[] = "vec4 Color;";
    constexpr const uint32_t pure_color_3d_mi_bytes = 16;

    constexpr FixedVertexEntry PURE_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
    };

    const FixedUBODescriptors PURE_COLOR_3D_UBOS = {
        {UBODescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::CameraInfo,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    const FixedSSBODescriptors PURE_COLOR_3D_SSBOS = {
        {SSBODescriptorSemantic::LocalToWorld,       uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {SSBODescriptorSemantic::TransformID,        uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstanceID, uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
        {SSBODescriptorSemantic::MaterialInstance,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
    };

    const FixedMaterialDef PURE_COLOR_3D_DEF {
        "PureColor3D",
        PrimitiveType::Triangles,
        PURE_COLOR_3D_VERTEX,
        uint32_t(sizeof(PURE_COLOR_3D_VERTEX) / sizeof(PURE_COLOR_3D_VERTEX[0])),
        &PURE_COLOR_3D_UBOS,
        &PURE_COLOR_3D_SSBOS,
        nullptr,
        pure_color_3d_mi_codes,
        pure_color_3d_mi_bytes,
    };
}

MaterialCreateInfo *CreatePureColor3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    MaterialVariantKey var_key;
    return CreateFromFixedDef3D("PureColor3D", profile, PURE_COLOR_3D_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl