#include"Build2DCommon.h"
#include"MaterialFactory2D.h"
#include<hgl/shadergen/CompositorCompiler.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

namespace hgl::graph::mtl{
MaterialCreateInfo *CreateVertexColor2D(const contract::PhysicalDeviceProfileLite *profile,
                                          const Material2DCreateConfig *cfg,
                                          const MaterialVariantDesc &desc,
                                          const MaterialVariantKey &key)
{
    if(!profile||!cfg)
        return(nullptr);

    auto vs_preamble = build2d::Build2DVertexPreamble(cfg, false, false);
    auto fs_preamble = build2d::Build2DFragmentPreamble(cfg, false, false);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);
    vertices.push_back({VAT_VEC4, VAN::Color});

    MaterialResourceManifest manifest;
    StaticMaterialDef def{};
    build2d::BuildBase2DFixedDef(def,
                                 "VertexColor2D",
                                 cfg,
                                 vertices,
                                 manifest);

    return CreateFromFixedDef2D("VertexColor2D", profile, def, key, vs_preamble, fs_preamble, cfg, desc);
}

static MaterialCreateInfo *VertexColor2D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{ return CreateVertexColor2D(profile, static_cast<const Material2DCreateConfig *>(cfg), *desc, key); }

}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(VertexColor2D, "VertexColor2D", hgl::graph::mtl::VertexColor2D_Adapter)
