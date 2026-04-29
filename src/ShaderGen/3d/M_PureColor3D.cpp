#include"MaterialFactory3DCommon.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry PURE_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
    };

    const UBOSemanticSet PURE_COLOR_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet PURE_COLOR_3D_SSBOS = build3d::MakeTransformSSBOs(true);

    const StaticMaterialDef PURE_COLOR_3D_DEF {
        "PureColor3D",
        PrimitiveType::Triangles,
        PURE_COLOR_3D_VERTEX,
        uint32_t(sizeof(PURE_COLOR_3D_VERTEX) / sizeof(PURE_COLOR_3D_VERTEX[0])),
        &PURE_COLOR_3D_UBOS,
        &PURE_COLOR_3D_SSBOS,
        nullptr,
        ShaderDataSchema::Color4f,
    };
}

MaterialCreateInfo *CreatePureColor3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    // [Step 3.5 T2] Single-entry routing via RouteKey().
    const MaterialVariantKey var_key = RouteKey(MaterialPreset::PureColor3D);
    return CreateFromFixedDef3D("PureColor3D", profile, PURE_COLOR_3D_DEF, var_key, cfg);
}

static MaterialCreateInfo *PureColor3D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{ return CreatePureColor3D(profile, static_cast<Material3DCreateConfig *>(cfg)); }
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(PureColor3D, "PureColor3D", hgl::graph::mtl::PureColor3D_Adapter)
