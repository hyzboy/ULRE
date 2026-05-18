#include"../3d/MaterialFactory3DCommon.h"
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

namespace hgl::graph::mtl{
namespace{

// ── Unified adapter (2D: position_format==VAT_VEC2, 3D: otherwise) ───────────────
static MaterialCreateInfo *PureColor_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{
    if (!profile || !cfg) return nullptr;

    auto *c3 = static_cast<Material3DCreateConfig *>(cfg);
    const bool is2D = (c3->position_format == VAT_VEC2);

    FixedVertexEntry v2[] = {{ VAT_VEC2, VAN::Position }};
    FixedVertexEntry v3[] = {{ VAT_VEC3, VAN::Position }};

    StaticMaterialDef def {
        "PureColor", c3->prim,
        is2D ? v2 : v3, 1,
        nullptr, nullptr, nullptr,
        ShaderDataSchema::Color4f,
    };
    return CreateFromFixedDef3D("PureColor", profile, def, key, c3, *desc);
}
}//anonymous
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(PureColor, "PureColor", hgl::graph::mtl::PureColor_Adapter)
