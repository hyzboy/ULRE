#include"MaterialFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char pure_color_3d_mi_codes[] = "vec4 Color;";
    constexpr const uint32_t pure_color_3d_mi_bytes = 16;

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
        pure_color_3d_mi_codes,
        pure_color_3d_mi_bytes,
    };
}

MaterialCreateInfo *CreatePureColor3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    const MaterialVariantKey var_key = build3d::MakeVariantKey();
    return CreateFromFixedDef3D("PureColor3D", profile, PURE_COLOR_3D_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl
