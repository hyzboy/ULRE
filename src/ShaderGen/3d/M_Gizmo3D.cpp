#include"MaterialFactory3DCommon.h"
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
        ShaderDataSchema::Color4f,
    };
}

MaterialCreateInfo *CreateGizmo3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    if(cfg)
        cfg->material_instance=true;    // Gizmo requires per-instance data

    // [Step 3.5 T2] PresetResolveTable supplies debug_shading=true.
    const MaterialVariantKey var_key = RouteKey(MaterialPreset::Gizmo3D);
    return CreateFromFixedDef3D("Gizmo3D", profile, GIZMO_3D_DEF, var_key, cfg);
}

static MaterialCreateInfo *Gizmo3D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{ return CreateGizmo3D(profile, static_cast<Material3DCreateConfig *>(cfg)); }
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(Gizmo3D, "Gizmo3D", hgl::graph::mtl::Gizmo3D_Adapter)
