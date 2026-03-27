#include"Build2DCommon.h"
#include"FixedDefFactory2D.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/MaterialLibrary.h>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateVertexColor2D(const contract::PhysicalDeviceProfileLite *profile,const Material2DCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    auto vs_preamble = build2d::Build2DVertexPreamble(cfg, false, false);
    auto fs_preamble = build2d::Build2DFragmentPreamble(cfg, false, false);

    const MaterialVariantKey var_key = MapPresetToVariantKey(MaterialPreset::VertexColor2D);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);
    vertices.push_back({VAT_VEC4, VertexInputRate::Vertex, VAN::Color});

    FixedUBODescriptors ubos;
    FixedSSBODescriptors ssbos;
    build2d::PushBaseUBODescriptors(ubos, cfg);
    build2d::PushBaseSSBODescriptors(ssbos, cfg);

    FixedMaterialDef def {
        "VertexColor2D",
        cfg->prim,
        vertices.data(), uint32_t(vertices.size()),
        &ubos,
        &ssbos,
        nullptr,
        nullptr, 0,
    };

    return CreateFromFixedDef2D("VertexColor2D", profile, def, var_key, vs_preamble, fs_preamble, cfg);
}

}//namespace hgl::graph::mtl
