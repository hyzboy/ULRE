#include"FixedDefFactory3D.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry VERTEX_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VertexInputRate::Vertex, VAN::Position },
        { VAT_VEC4, VertexInputRate::Vertex, VAN::Color },
    };

    const FixedUBODescriptors VERTEX_COLOR_3D_UBOS = {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo,
    };

    const FixedSSBODescriptors VERTEX_COLOR_3D_SSBOS = {
        SSBODescriptorSemantic::LocalToWorld,
        SSBODescriptorSemantic::TransformID,
    };

    const FixedMaterialDef VERTEX_COLOR_3D_DEF {
        "VertexColor3D",
        PrimitiveType::Triangles,
        VERTEX_COLOR_3D_VERTEX,
        uint32_t(sizeof(VERTEX_COLOR_3D_VERTEX) / sizeof(VERTEX_COLOR_3D_VERTEX[0])),
        &VERTEX_COLOR_3D_UBOS,
        &VERTEX_COLOR_3D_SSBOS,
        nullptr,
        nullptr,
        0,
    };
}

MaterialCreateInfo *CreateVertexColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg)
{
    MaterialVariantKey var_key;
    var_key.SetVertexAttribEnabled(VertexAttrib::Color);
    return CreateFromFixedDef3D("VertexColor3D", profile, VERTEX_COLOR_3D_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl
