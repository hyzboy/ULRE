#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/SamplerName.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreatePureTexture2D(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    auto vs_preamble = build2d::Build2DVertexPreamble(cfg, true, false);
    auto fs_preamble = build2d::Build2DFragmentPreamble(cfg, true, false);

    const MaterialVariantKey var_key = MapPresetToVariantKey(MaterialPreset::PureTexture2D);
    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariant(var_key);
    if (!var_desc)
    {
        std::fprintf(stderr, "[PureTexture2D] VariantRegistry lookup failed\n");
        return nullptr;
    }

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(var_key, *var_desc);
    if (!result.success)
    {
        std::fprintf(stderr, "[PureTexture2D] CompositorAssembler failed: %s\n", result.error_message.c_str());
        return nullptr;
    }

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, cfg);
    vertices.push_back({VAT_VEC2, VertexInputRate::Vertex, VAN::TexCoord});

    FixedUBODescriptors ubos;
    FixedSSBODescriptors ssbos;
    FixedTextureSamplerDescriptors samplers;
    build2d::PushBaseUBODescriptors(ubos, cfg);
    build2d::PushBaseSSBODescriptors(ssbos, cfg);
    AddFixedTextureSampler(samplers,
                           SamplerSlot::BaseColor,
                           uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                           SamplerType::Sampler2D);

    FixedMaterialDef def {
        "PureTexture2D",
        cfg->prim,
        vertices.data(), uint32_t(vertices.size()),
        &ubos,
        &ssbos,
        &samplers,
        nullptr, 0,
    };

    std::string vs = vs_preamble + result.vertex_glsl;
    std::string fs = fs_preamble + result.fragment_glsl;

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[PureTexture2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
