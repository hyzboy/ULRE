#include"../3d/MaterialFactory3DCommon.h"
#include"../3d/Build3DCommon.h"
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

namespace hgl::graph::mtl{
namespace{

// ── 3D path ───────────────────────────────────────────────────────────────────────
constexpr FixedVertexEntry kVertexColor3DVertex[] = {
    { VAT_VEC3, VAN::Position },
    { VAT_VEC4, VAN::Color    },
};
const UBOSemanticSet  kVertexColor3DUBOs  = build3d::MakeViewportCameraUBOs();
const SSBOSemanticSet kVertexColor3DSSBOs = build3d::MakeTransformSSBOs(false);
const StaticMaterialDef kVertexColor3DDef {
    "VertexColor", PrimitiveType::Triangles,
    kVertexColor3DVertex, 2,
    &kVertexColor3DUBOs, &kVertexColor3DSSBOs, nullptr,
    ShaderDataSchema::None,
};

// ── Unified adapter ──────────────────────────────────────────────────────────────
static MaterialCreateInfo *VertexColor_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{
    if (!profile || !cfg) return nullptr;

    if (cfg->kind == ConfigKind::D2)
    {
        const Material2DCreateConfig *c2 = static_cast<const Material2DCreateConfig *>(cfg);
        Material3DCreateConfig cfg3d(c2->prim, IncludeCamera::Without, IncludeL2W::Without, IncludeSky::Without);
        cfg3d.position_format = VAT_VEC2;
        cfg3d.local_to_world  = c2->local_to_world;
        cfg3d.coord_2d        = c2->coordinate_system;
        cfg3d.preset_name     = c2->preset_name;
        MaterialVariantDesc e = *desc; e.coord_2d = c2->coordinate_system;
        FixedVertexEntry v[] = {{ VAT_VEC2, VAN::Position }, { VAT_VEC4, VAN::Color }};
        StaticMaterialDef def { "VertexColor", cfg3d.prim, v, 2, nullptr, nullptr, nullptr, ShaderDataSchema::None };
        return CreateFromFixedDef3D("VertexColor", profile, def, key, &cfg3d, e);
    }
    else
    {
        return CreateFromFixedDef3D("VertexColor", profile, kVertexColor3DDef, key,
                                    static_cast<Material3DCreateConfig *>(cfg), *desc);
    }
}
}//anonymous
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(VertexColor, "VertexColor", hgl::graph::mtl::VertexColor_Adapter)
