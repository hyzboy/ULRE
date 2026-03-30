#include"MaterialFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/math/Vector.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char VERTEX_LUMINANCE_3D_MI_CODES[] = "vec4 Color;";
    constexpr const uint32_t VERTEX_LUMINANCE_3D_MI_BYTES = sizeof(hgl::math::Vector4f);

    constexpr FixedVertexEntry VERTEX_LUMINANCE_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_FLOAT, VAN::Luminance },
    };

    const UBOSemanticSet VERTEX_LUMINANCE_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet VERTEX_LUMINANCE_3D_SSBOS = build3d::MakeTransformSSBOs(true);

    const StaticMaterialDef VERTEX_LUMINANCE_3D_DEF {
        "VertexLuminance3D",
        PrimitiveType::Triangles,
        VERTEX_LUMINANCE_3D_VERTEX,
        uint32_t(sizeof(VERTEX_LUMINANCE_3D_VERTEX) / sizeof(VERTEX_LUMINANCE_3D_VERTEX[0])),
        &VERTEX_LUMINANCE_3D_UBOS,
        &VERTEX_LUMINANCE_3D_SSBOS,
        nullptr,
        VERTEX_LUMINANCE_3D_MI_CODES,
        VERTEX_LUMINANCE_3D_MI_BYTES,
    };
}

MaterialCreateInfo *CreateVertexLuminance3D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    cfg->material_instance=true;

    const MaterialVariantKey var_key = build3d::MakeVariantKeyWithAttrib(VertexAttrib::Luminance);
    return CreateFromFixedDef3D("VertexLuminance3D", profile, VERTEX_LUMINANCE_3D_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl
