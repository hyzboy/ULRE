#include"Build2DCommon.h"
#include"MaterialFactory2D.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/MaterialLibrary.h>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreatePureColor2D(const contract::PhysicalDeviceProfileLite *profile,
                                        Material2DCreateConfig *cfg,
                                        const MaterialVariantKey &key)
{
    if(!profile||!cfg)
        return(nullptr);

    cfg->material_instance=true;

    auto vs_preamble = build2d::Build2DVertexPreamble(cfg, false, true);
    auto fs_preamble = build2d::Build2DFragmentPreamble(cfg, false, true);

    // Build DEF dynamically
    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);

    StaticMaterialDef def{};
    UBOSemanticSet ubos;
    SSBOSemanticSet ssbos;
    build2d::BuildBase2DFixedDef(def,
                                 "PureColor2D",
                                 cfg,
                                 vertices,
                                 ubos,
                                 ssbos,
                                 nullptr,
                                 ShaderDataSchema::Color4f);

    return CreateFromFixedDef2D("PureColor2D", profile, def, key, vs_preamble, fs_preamble, cfg);
}
}//namespace hgl::graph::mtl
