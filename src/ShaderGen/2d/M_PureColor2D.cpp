#include"Build2DCommon.h"
#include"MaterialFactory2D.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include <memory>

namespace hgl::graph::mtl{

std::unique_ptr<MaterialCreateInfo> CreatePureColor2DOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                           Material2DCreateConfig *cfg,
                                                           const MaterialVariantDesc &desc,
                                                           const MaterialVariantKey &key)
{
    if(!profile||!cfg)
        return nullptr;

    cfg->material_instance=true;

    auto vs_preamble = build2d::Build2DVertexPreamble(cfg, false, true);
    auto fs_preamble = build2d::Build2DFragmentPreamble(cfg, false, true);

    // Build DEF dynamically
    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);

    MaterialResourceManifest manifest;
    StaticMaterialDef def{};
    build2d::BuildBase2DFixedDef(def,
                                 "PureColor2D",
                                 cfg,
                                 vertices,
                                 manifest,
                                 ShaderDataSchema::Color4f);

    return CreateFromFixedDef2DOwned("PureColor2D", profile, def, key, vs_preamble, fs_preamble, cfg, desc);
}

static std::unique_ptr<MaterialCreateInfo> PureColor2D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{ return CreatePureColor2DOwned(profile, static_cast<Material2DCreateConfig *>(cfg), *desc, key); }
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(PureColor2D, "PureColor2D", hgl::graph::mtl::PureColor2D_Adapter)
