#include"MaterialFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/UBOCommon.h>
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr VertexAttributeSpec VERTEX_PATTLE_COLOR_3D_VERTEX_SPECS[] = {
        { VAN::Position, VAT_VEC3, PF_RGB32F },
        { VAN::Color,    VAT_UINT, PF_R32U   },
    };

    const UBOSemanticSet VERTEX_PATTLE_COLOR_3D_UBOS = []()
    {
        UBOSemanticSet descriptors = build3d::MakeViewportCameraUBOs();
        descriptors.insert(UBODescriptorSemantic::ColorPattle);
        return descriptors;
    }();

    const SSBOSemanticSet VERTEX_PATTLE_COLOR_3D_SSBOS = build3d::MakeTransformSSBOs(false);

    const StaticMaterialDef VERTEX_PATTLE_COLOR_3D_DEF {
        "VertexPattleColor3D",
        PrimitiveType::Triangles,
        nullptr,
        0,
        &VERTEX_PATTLE_COLOR_3D_UBOS,
        &VERTEX_PATTLE_COLOR_3D_SSBOS,
        nullptr,
        nullptr,
        0,
        InstanceDataLayout::None,
        VERTEX_PATTLE_COLOR_3D_VERTEX_SPECS,
        uint32_t(sizeof(VERTEX_PATTLE_COLOR_3D_VERTEX_SPECS) / sizeof(VERTEX_PATTLE_COLOR_3D_VERTEX_SPECS[0])),
    };
}//namespace

MaterialCreateInfo *CreateVertexPattleColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg)
{
    Material3DCreateConfig local_cfg = build3d::MakeLocalConfig(cfg);

    const MaterialVariantKey var_key = build3d::MakeVariantKeyWithAttribAndDebug(VertexAttrib::Color);
    return CreateFromFixedDef3D("VertexPattleColor3D", profile, VERTEX_PATTLE_COLOR_3D_DEF, var_key, &local_cfg);
}
}//namespace hgl::graph::mtl
