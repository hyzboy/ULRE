#include"MaterialFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/UBOCommon.h>
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry VERTEX_PATTLE_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_UINT, VAN::Color },
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
    Material3DCreateConfig local_cfg = build3d::MakeLocalConfig(cfg);

    const MaterialVariantKey var_key = build3d::MakeVariantKeyWithAttribAndDebug(VertexAttrib::Color);
    return CreateFromFixedDef3D("VertexPattleColor3D", profile, VERTEX_PATTLE_COLOR_3D_DEF, var_key, &local_cfg);
}
}//namespace hgl::graph::mtl
