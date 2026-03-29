#include"FixedDefFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/math/Vector.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr const char VERTEX_LUMINANCE_2D_MI_CODES[] = "vec4 Color;";
    constexpr const uint32_t VERTEX_LUMINANCE_2D_MI_BYTES = sizeof(hgl::math::Vector4f);

    constexpr FixedVertexEntry VERTEX_LUMINANCE_2D_VERTEX[] = {
        { VAT_VEC2, VAN::Position },
        { VAT_FLOAT, VAN::Luminance },
    };

    const FixedUBODescriptors VERTEX_LUMINANCE_2D_UBOS = build3d::MakeViewportCameraUBOs();

    const FixedSSBODescriptors VERTEX_LUMINANCE_2D_SSBOS = build3d::MakeTransformSSBOs(true);

    const FixedMaterialDef VERTEX_LUMINANCE_2D_DEF {
        "VertexLuminance2D",
        PrimitiveType::Triangles,
        VERTEX_LUMINANCE_2D_VERTEX,
        uint32_t(sizeof(VERTEX_LUMINANCE_2D_VERTEX) / sizeof(VERTEX_LUMINANCE_2D_VERTEX[0])),
        &VERTEX_LUMINANCE_2D_UBOS,
        &VERTEX_LUMINANCE_2D_SSBOS,
        nullptr,
        VERTEX_LUMINANCE_2D_MI_CODES,
        VERTEX_LUMINANCE_2D_MI_BYTES,
    };
}

MaterialCreateInfo *CreateVertexLuminance2D(const contract::PhysicalDeviceProfileLite *profile,Material3DCreateConfig *cfg)
{
    cfg->material_instance=true;

    MaterialVariantKey var_key = build3d::MakeVariantKeyWithAttrib(VertexAttrib::Luminance);
    var_key.SetVertexAttribEnabled(VertexAttrib::Position);
    return CreateFromFixedDef3D("VertexLuminance2D", profile, VERTEX_LUMINANCE_2D_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl
