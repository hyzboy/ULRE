#include"MaterialFactory3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{
namespace
{
    constexpr FixedVertexEntry SKY_MINIMAL_VERTEX[] = {
        { VAT_VEC3, VAN::Position },
    };

    const StaticMaterialDef SKY_MINIMAL_DEF {
        "SkyMinimal",
        PrimitiveType::Triangles,
        SKY_MINIMAL_VERTEX,
        uint32_t(sizeof(SKY_MINIMAL_VERTEX) / sizeof(SKY_MINIMAL_VERTEX[0])),
        nullptr, nullptr, nullptr,
        ShaderDataSchema::None
    };
    static MaterialCreateInfo *CreateSkyMinimalFactory(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialVariantDesc                 *desc,
        const MaterialVariantKey                  &key,
        MaterialCreateConfig                      *cfg)
    {
        return CreateFromFixedDef3D("SkyMinimal",
                                    profile,
                                    SKY_MINIMAL_DEF,
                                    key,
                                    static_cast<const SkyMinimalCreateConfig *>(cfg),
                                    *desc);
    }
}//namespace
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(SkyMinimal, "SkyMinimal", hgl::graph::mtl::CreateSkyMinimalFactory)
