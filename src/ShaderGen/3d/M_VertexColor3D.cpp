#include"MaterialFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr VertexAttributeSpec VERTEX_COLOR_3D_VERTEX_SPECS[] = {
        { VAN::Position, VAT_VEC3, PF_RGB32F  },
        { VAN::Color,    VAT_VEC4, PF_RGBA32F },
    };

    const UBOSemanticSet VERTEX_COLOR_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet VERTEX_COLOR_3D_SSBOS = build3d::MakeTransformSSBOs(false);

    const StaticMaterialDef VERTEX_COLOR_3D_DEF {
        "VertexColor3D",
        PrimitiveType::Triangles,
        nullptr,
        0,
        &VERTEX_COLOR_3D_UBOS,
        &VERTEX_COLOR_3D_SSBOS,
        nullptr,
        nullptr,
        0,
        InstanceDataLayout::None,
        VERTEX_COLOR_3D_VERTEX_SPECS,
        uint32_t(sizeof(VERTEX_COLOR_3D_VERTEX_SPECS) / sizeof(VERTEX_COLOR_3D_VERTEX_SPECS[0])),
    };
}

MaterialCreateInfo *CreateVertexColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg)
{
    const MaterialVariantKey var_key = build3d::MakeVariantKeyWithAttrib(VertexAttrib::Color);
    return CreateFromFixedDef3D("VertexColor3D", profile, VERTEX_COLOR_3D_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl
