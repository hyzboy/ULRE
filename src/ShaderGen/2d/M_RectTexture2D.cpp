#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateRectTexture2D(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    mtl::Material2DCreateConfig inner=*cfg;
    inner.prim=PrimitiveType::Triangles;
    inner.position_format=VK_FORMAT_R32G32_SFLOAT;
    inner.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    // Build DEF
    auto preamble = build2d::Build2DPreamble(&inner, true, false);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &inner);
    vertices.push_back({ VK_FORMAT_R32G32_SFLOAT, VertexSemantic::TexCoord });

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, &inner);
    descriptors.push_back({DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), SamplerName::BaseColor, nullptr, "sampler2D", DescriptorSemantic::MaterialSampler, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::Sampler});

    FixedMaterialDef def {
        "RectTexture2D",
        inner.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        nullptr, 0,
    };

    std::string vs = preamble + "#include \"2d/puretexture2d.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/puretexture2d.frag.glsl\"\n";

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, &inner);
    if(!mci)
        std::fprintf(stderr, "[RectTexture2D] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl

