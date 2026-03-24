#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreatePureTexture2D(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
{
    auto preamble = build2d::Build2DPreamble(cfg, true, false);

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

    std::string vs = preamble + "#include \"2d/puretexture2d.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/puretexture2d.frag.glsl\"\n";

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, cfg);
    if(!mci)
        std::fprintf(stderr, "[PureTexture2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl
