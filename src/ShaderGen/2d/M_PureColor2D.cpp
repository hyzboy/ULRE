#include"../3d/MaterialFactory3DCommon.h"
#include"../3d/Build3DCommon.h"
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

namespace hgl::graph::mtl{
namespace{

// ── 3D path ───────────────────────────────────────────────────────────────────────
constexpr FixedVertexEntry kPureColor3DVertex[] = {
    { VAT_VEC3, VAN::Position },
};
const UBOSemanticSet  kPureColor3DUBOs  = build3d::MakeViewportCameraUBOs();
const SSBOSemanticSet kPureColor3DSSBOs = build3d::MakeTransformSSBOs(true);
const StaticMaterialDef kPureColor3DDef {
    "PureColor", PrimitiveType::Triangles,
    kPureColor3DVertex, 1,
    &kPureColor3DUBOs, &kPureColor3DSSBOs, nullptr,
    ShaderDataSchema::Color4f,
};

// ── Unified adapter ──────────────────────────────────────────────────────────────
static MaterialCreateInfo *PureColor_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{
    if (!profile || !cfg) return nullptr;

    if (cfg->kind == ConfigKind::D2)
    {
        const Material2DCreateConfig *c2 = static_cast<const Material2DCreateConfig *>(cfg);
        Material3DCreateConfig cfg3d(c2->prim, IncludeL2W::Without);
        cfg3d.position_format   = VAT_VEC2;
        cfg3d.local_to_world    = c2->local_to_world;
        cfg3d.coord_2d          = c2->coordinate_system;
        cfg3d.preset_name       = c2->preset_name;
        cfg3d.material_instance = true;
        MaterialVariantDesc e = *desc; e.coord_2d = c2->coordinate_system;
        FixedVertexEntry v[] = {{ VAT_VEC2, VAN::Position }};
        StaticMaterialDef def { "PureColor", cfg3d.prim, v, 1, nullptr, nullptr, nullptr, ShaderDataSchema::Color4f };
        return CreateFromFixedDef3D("PureColor", profile, def, key, &cfg3d, e);
    }
    else
    {
        auto *c3 = static_cast<Material3DCreateConfig *>(cfg);
        return CreateFromFixedDef3D("PureColor", profile, kPureColor3DDef, key, c3, *desc);
    }
}
}//anonymous
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(PureColor, "PureColor", hgl::graph::mtl::PureColor_Adapter)
