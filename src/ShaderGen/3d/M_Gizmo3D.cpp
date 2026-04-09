#include"MaterialFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/math/Vector.h>

namespace hgl::graph::mtl
{
namespace
{
    constexpr VertexAttributeSpec GIZMO_3D_VERTEX_SPECS[] = {
        { VAN::Position, VAT_VEC3, PF_RGB32F },
        { VAN::Normal,   VAT_VEC3, PF_RGB32F },
    };

    const UBOSemanticSet GIZMO_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet GIZMO_3D_SSBOS = build3d::MakeTransformSSBOs(true);

    const StaticMaterialDef GIZMO_3D_DEF {
        "Gizmo3D",
        PrimitiveType::Triangles,
        nullptr,
        0,
        &GIZMO_3D_UBOS,
        &GIZMO_3D_SSBOS,
        nullptr,
        nullptr, 0,
        InstanceDataLayout::Color4f,
        GIZMO_3D_VERTEX_SPECS,
        uint32_t(sizeof(GIZMO_3D_VERTEX_SPECS) / sizeof(GIZMO_3D_VERTEX_SPECS[0])),
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
