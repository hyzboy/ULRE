#include"MaterialFactory3D.h"
#include"Build3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr VertexAttributeSpec SKY_MINIMAL_VERTEX_SPECS[] = {
        { VAN::Position, VAT_VEC3, PF_RGB32F },
    };

    const UBOSemanticSet SKY_MINIMAL_UBOS = build3d::MakeViewportCameraSkyUBOs();

    const SSBOSemanticSet SKY_MINIMAL_SSBOS = build3d::MakeTransformSSBOs(false);

    const StaticMaterialDef SKY_MINIMAL_DEF {
        "SkyMinimal",
        PrimitiveType::Triangles,
        nullptr,
        0,
        &SKY_MINIMAL_UBOS,
        &SKY_MINIMAL_SSBOS,
        nullptr,
        nullptr,
        0,
        InstanceDataLayout::None,
        SKY_MINIMAL_VERTEX_SPECS,
        uint32_t(sizeof(SKY_MINIMAL_VERTEX_SPECS) / sizeof(SKY_MINIMAL_VERTEX_SPECS[0])),
    };
}//namespace

MaterialCreateInfo *CreateSkyMinimal(const contract::PhysicalDeviceProfileLite *profile, const SkyMinimalCreateConfig *cfg)
{
    const MaterialVariantKey var_key = build3d::MakeVariantKeyWithSurface(MaterialSurfaceClass::Sky);
    return CreateFromFixedDef3D("SkyMinimal", profile, SKY_MINIMAL_DEF, var_key, cfg);
}
}//namespace hgl::graph::mtl
