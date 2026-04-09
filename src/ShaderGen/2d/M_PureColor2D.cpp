#include"Build2DCommon.h"
#include"MaterialFactory2D.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/math/Vector.h>
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
    std::vector<VertexAttributeSpec> specs;
    build2d::PushBaseVertexSpecs(specs, cfg);

    StaticMaterialDef def{};
    UBOSemanticSet ubos;
    SSBOSemanticSet ssbos;
    build2d::BuildBase2DSpecDef(def,
                                "PureColor2D",
                                cfg,
                                specs,
                                ubos,
                                ssbos,
                                nullptr,
                                InstanceDataLayout::Color4f);

    return CreateFromFixedDef2D("PureColor2D", profile, def, key, vs_preamble, fs_preamble, cfg);
}
}//namespace hgl::graph::mtl
