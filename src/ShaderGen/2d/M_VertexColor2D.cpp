#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateVertexColor2D(const contract::PhysicalDeviceProfileLite *profile,const Material2DCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    auto vs_preamble = build2d::Build2DVertexPreamble(cfg, false, false);
    auto fs_preamble = build2d::Build2DFragmentPreamble(cfg, false, false);

    const MaterialVariantKey var_key = MapPresetToVariantKey(MaterialPreset::VertexColor2D);
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[VertexColor2D] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(var_key, *var_desc);
    if (!result.success)
    {
        std::fprintf(stderr, "[VertexColor2D] CompositorAssembler failed: %s\n", result.error_message.c_str());
        return nullptr;
    }

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

    std::string vs = vs_preamble + result.vertex_glsl;
    std::string fs = fs_preamble + result.fragment_glsl;

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[VertexColor2D] CompileCompositorMaterial failed\n");
    return mci;
}

}//namespace hgl::graph::mtl
