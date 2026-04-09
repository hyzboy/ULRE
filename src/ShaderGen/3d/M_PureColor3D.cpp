#include"MaterialFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr VertexAttributeSpec PURE_COLOR_3D_VERTEX_SPECS[] = {
        { VAN::Position, VAT_VEC3, PF_RGB32F },
    };

    const UBOSemanticSet PURE_COLOR_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet PURE_COLOR_3D_SSBOS = build3d::MakeTransformSSBOs(true);

    const StaticMaterialDef PURE_COLOR_3D_DEF {
        "PureColor3D",
        PrimitiveType::Triangles,
        nullptr,
        0,
        &PURE_COLOR_3D_UBOS,
        &PURE_COLOR_3D_SSBOS,
        nullptr,
        nullptr, 0,
        InstanceDataLayout::Color4f,
        PURE_COLOR_3D_VERTEX_SPECS,
        uint32_t(sizeof(PURE_COLOR_3D_VERTEX_SPECS) / sizeof(PURE_COLOR_3D_VERTEX_SPECS[0])),
    };
}

MaterialCreateInfo *CreatePureColor3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    const MaterialVariantKey var_key = build3d::MakeVariantKey();
    return CreateFromFixedDef3D("PureColor3D", profile, PURE_COLOR_3D_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl
