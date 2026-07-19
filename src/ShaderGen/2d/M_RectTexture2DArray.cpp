#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/mtl/SamplerName.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreateRectTexture2DArray(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
{
    constexpr const char mi_codes[]="uvec4 id;";
    constexpr const uint32_t mi_bytes=sizeof(math::Vector4u);
    if(!cfg)
        return(nullptr);

    mtl::Material2DCreateConfig inner=*cfg;
    inner.prim=PrimitiveType::Triangles;
    inner.material_instance=true;
    inner.position_format=VAT_VEC2;
    inner.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    // Build DEF
    auto preamble = build2d::Build2DPreamble(&inner, true, true);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &inner);
    vertices.push_back({ VAT_VEC2, VAN::TexCoord });

    std::vector<FixedDescriptorEntry> descriptors;
    build2d::PushBaseDescriptorEntries(descriptors, &inner);
    descriptors.push_back({DescriptorSetType::Material, DescriptorKind::TextureSampler, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT), SamplerName::BaseColor, nullptr, "sampler2DArray", DescriptorSemantic::MaterialSampler, TextureSlot::BaseColor, DataSlot::PBRSurface, SSBOType::UserDefined, DescriptorSemanticLayer::Sampler});

    FixedMaterialDef def {
        "RectTexture2DArray",
        inner.prim,
        vertices.data(), uint32_t(vertices.size()),
        descriptors.data(), uint32_t(descriptors.size()),
        mi_codes, mi_bytes,
    };

    std::string vs = preamble + "#include \"2d/recttexture2darray.vert.glsl\"\n";
    std::string fs = preamble + "#include \"2d/recttexture2darray.frag.glsl\"\n";

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, &inner);
    if(!mci)
        std::fprintf(stderr, "[RectTexture2DArray] CompileCompositorMaterial failed\n");
    return mci;
}
}//namespace hgl::graph::mtl

