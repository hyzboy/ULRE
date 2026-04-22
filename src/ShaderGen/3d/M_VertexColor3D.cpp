#include"MaterialFactory3DCommon.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry VERTEX_COLOR_3D_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
        { VAT_VEC4, VAN::Color },
    };

    const UBOSemanticSet VERTEX_COLOR_3D_UBOS = build3d::MakeViewportCameraUBOs();

    const SSBOSemanticSet VERTEX_COLOR_3D_SSBOS = build3d::MakeTransformSSBOs(false);

    const StaticMaterialDef VERTEX_COLOR_3D_DEF {
        "VertexColor3D",
        PrimitiveType::Triangles,
        VERTEX_COLOR_3D_VERTEX,
        uint32_t(sizeof(VERTEX_COLOR_3D_VERTEX) / sizeof(VERTEX_COLOR_3D_VERTEX[0])),
        &VERTEX_COLOR_3D_UBOS,
        &VERTEX_COLOR_3D_SSBOS,
        nullptr,
        ShaderDataSchema::None
    };
}

MaterialCreateInfo *CreateVertexColor3D(const contract::PhysicalDeviceProfileLite *profile,const Material3DCreateConfig *cfg)
{
    const MaterialVariantKey var_key = build3d::MakeVariantKeyWithAttrib(VertexAttrib::Color);
    return CreateFromFixedDef3D("VertexColor3D", profile, VERTEX_COLOR_3D_DEF, var_key, cfg);
}

static MaterialCreateInfo *VertexColor3D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantKey &,
    MaterialCreateConfig *cfg)
{ return CreateVertexColor3D(profile, static_cast<const Material3DCreateConfig *>(cfg)); }
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(VertexColor3D, "VertexColor3D", hgl::graph::mtl::VertexColor3D_Adapter)
