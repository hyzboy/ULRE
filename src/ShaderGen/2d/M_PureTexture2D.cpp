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
    vertices.push_back({ VAT_VEC2, VAN::TexCoord });

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, cfg);
    descriptors.push_back({DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), SamplerName::BaseColor, nullptr, "sampler2D", DescriptorSemantic::MaterialSampler, TextureSlot::BaseColor});

    FixedMaterialDef def {
        "PureTexture2D",
        cfg->prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
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
