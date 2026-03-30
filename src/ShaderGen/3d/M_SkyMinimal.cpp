#include"MaterialFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry SKY_MINIMAL_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
    };

    const UBOSemanticSet SKY_MINIMAL_UBOS = build3d::MakeViewportCameraSkyUBOs();

    const SSBOSemanticSet SKY_MINIMAL_SSBOS = build3d::MakeTransformSSBOs(false);

    const StaticMaterialDef SKY_MINIMAL_DEF {
        "SkyMinimal",
        PrimitiveType::Triangles,
        SKY_MINIMAL_VERTEX,
        uint32_t(sizeof(SKY_MINIMAL_VERTEX) / sizeof(SKY_MINIMAL_VERTEX[0])),
        &SKY_MINIMAL_UBOS,
        &SKY_MINIMAL_SSBOS,
        nullptr,
        nullptr,
        0,
    };
}//namespace

MaterialCreateInfo *CreateSkyMinimal(const contract::PhysicalDeviceProfileLite *profile, const SkyMinimalCreateConfig *cfg)
{
    const MaterialVariantKey var_key = build3d::MakeVariantKeyWithSurface(SurfaceType::Sky);
    return CreateFromFixedDef3D("SkyMinimal", profile, SKY_MINIMAL_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl
