#include"Build2DCommon.h"
#include"MaterialFactory2D.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/MaterialLibrary.h>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateVertexColor2D(const contract::PhysicalDeviceProfileLite *profile,
                                          const Material2DCreateConfig *cfg,
                                          const MaterialVariantKey &key)
{
    if(!profile||!cfg)
        return(nullptr);

    auto vs_preamble = build2d::Build2DVertexPreamble(cfg, false, false);
    auto fs_preamble = build2d::Build2DFragmentPreamble(cfg, false, false);

    std::vector<VertexAttributeSpec> specs;
    build2d::PushBaseVertexSpecs(specs, cfg);
    specs.push_back(MakeLegacyVertexAttributeSpec(VAT_VEC4, VAN::Color));

    StaticMaterialDef def{};
    UBOSemanticSet ubos;
    SSBOSemanticSet ssbos;
    build2d::BuildBase2DSpecDef(def,
                                "VertexColor2D",
                                cfg,
                                specs,
                                ubos,
                                ssbos);

    return CreateFromFixedDef2D("VertexColor2D", profile, def, key, vs_preamble, fs_preamble, cfg);
}

}//namespace hgl::graph::mtl
