#include"FixedDefFactory3D.h"
#include<hgl/mtl/UBOCommon.h>
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry VERTEX_PATTLE_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
        { VAT_UINT, VertexInputRate::Vertex, VAN::Color },
    };

    const FixedUBODescriptors VERTEX_PATTLE_COLOR_3D_UBOS = {
        {UBODescriptorSemantic::ViewportInfo, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::CameraInfo,   uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {UBODescriptorSemantic::ColorPattle,  uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
    };

    const FixedSSBODescriptors VERTEX_PATTLE_COLOR_3D_SSBOS = {
        {SSBODescriptorSemantic::LocalToWorld, uint32_t(VK_SHADER_STAGE_ALL_GRAPHICS)},
        {SSBODescriptorSemantic::TransformID,  uint32_t(VK_SHADER_STAGE_VERTEX_BIT)},
    };

    const FixedMaterialDef VERTEX_PATTLE_COLOR_3D_DEF {
        "VertexPattleColor3D",
        PrimitiveType::Triangles,
        VERTEX_PATTLE_COLOR_3D_VERTEX,
        uint32_t(sizeof(VERTEX_PATTLE_COLOR_3D_VERTEX) / sizeof(VERTEX_PATTLE_COLOR_3D_VERTEX[0])),
        &VERTEX_PATTLE_COLOR_3D_UBOS,
        &VERTEX_PATTLE_COLOR_3D_SSBOS,
        nullptr,
        nullptr,
        0,
    };
}//namespace

MaterialCreateInfo *CreateVertexPattleColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig local_cfg = cfg ? *cfg : Material3DCreateConfig();

    MaterialVariantKey var_key;
    var_key.SetVertexAttribEnabled(VertexAttrib::Color);
    var_key.SetDebugShading(true);
    return CreateFromFixedDef3D("VertexPattleColor3D", profile, VERTEX_PATTLE_COLOR_3D_DEF, var_key, &local_cfg);
}
}//namespace hgl::graph::mtl