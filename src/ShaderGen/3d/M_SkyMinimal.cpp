#include"MaterialFactory3DCommon.h"
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
        ShaderDataSchema::None
    };
}//namespace

MaterialCreateInfo *CreateSkyMinimal(const contract::PhysicalDeviceProfileLite *profile, const SkyMinimalCreateConfig *cfg,
                                     const MaterialVariantDesc &desc, const MaterialVariantKey &key)
{
    return CreateFromFixedDef3D("SkyMinimal", profile, SKY_MINIMAL_DEF, key, cfg, desc);
}

static MaterialCreateInfo *SkyMinimal_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{ return CreateSkyMinimal(profile, static_cast<const SkyMinimalCreateConfig *>(cfg), *desc, key); }
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(SkyMinimal, "SkyMinimal", hgl::graph::mtl::SkyMinimal_Adapter)
