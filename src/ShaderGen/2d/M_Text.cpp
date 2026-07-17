#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateText2D(const contract::PhysicalDeviceProfileLite *profile,const Text2DMaterialCreateConfig *cfg)
{
    constexpr const char mi_codes[]="uint TextColor;";
    constexpr const uint32_t mi_bytes=sizeof(uint32);
    if(!profile||!cfg)
        return(nullptr);

    Text2DMaterialCreateConfig new_cfg=*cfg;
    new_cfg.prim=PrimitiveType::Triangles;
    new_cfg.position_format=VAT_IVEC2;
    new_cfg.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    // Build DEF
    auto preamble = build2d::Build2DPreamble(&new_cfg, true, true);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &new_cfg);
    vertices.push_back({ VAT_VEC2, VAN::TexCoord });

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, &new_cfg);
    descriptors.push_back({DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), SamplerName::Text, nullptr, "sampler2D"});

    FixedMaterialDef def {
        "Text2D",
        new_cfg.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        mi_codes, mi_bytes,
    };

    std::string vs = preamble + "#include \"2d/text2d.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/text2d.frag.glsl\"\n";

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, &new_cfg);
    if(!mci)
        std::fprintf(stderr, "[Text2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl

