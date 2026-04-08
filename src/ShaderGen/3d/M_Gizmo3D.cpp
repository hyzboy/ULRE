#include"MaterialFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/math/Vector.h>

namespace hgl::graph::mtl
{
namespace
{
    constexpr FixedVertexEntry GIZMO_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC3, VAN::Normal },
    };

    const UBOSemanticSet GIZMO_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet GIZMO_3D_SSBOS = build3d::MakeTransformSSBOs(true);

    const StaticMaterialDef GIZMO_3D_DEF {
        "Gizmo3D",
        PrimitiveType::Triangles,
        GIZMO_3D_VERTEX,
        uint32_t(sizeof(GIZMO_3D_VERTEX) / sizeof(GIZMO_3D_VERTEX[0])),
        &GIZMO_3D_UBOS,
        &GIZMO_3D_SSBOS,
        nullptr,
        nullptr, 0,
        InstanceDataLayout::Color4f,
    };
}

MaterialCreateInfo *CreateGizmo3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    if(cfg)
        cfg->material_instance=true;

    MaterialVariantKey var_key = build3d::MakeVariantKey();
    var_key.SetDebugShading(true);
    return CreateFromFixedDef3D("Gizmo3D", profile, GIZMO_3D_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl
