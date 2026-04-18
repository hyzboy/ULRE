#include"Build2DCommon.h"
#include"MaterialFactory2D.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/math/Vector.h>
#include<hgl/mtl/MaterialLibrary.h>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreatePureTextureVariant(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialVariantKey &key,
                                             const mtl::Material2DCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    const TextureSourceMode mode = key.GetTextureSourceMode(SamplerSlot::BaseColor);
    const bool use_array = (mode == TextureSourceMode::Array);

    mtl::Material2DCreateConfig inner=*cfg;
    inner.prim=PrimitiveType::Triangles;
    inner.material_instance=use_array;
    inner.position_format=VAT_VEC2;
    inner.shader_stage_flag_bit&=~(uint32_t)ShaderStage::Geometry;

    auto vs_preamble = build2d::Build2DVertexPreamble(&inner,
                                                      true,
                                                      use_array,
                                                      SamplerSlot::BaseColor,
                                                      use_array);
    auto fs_preamble = build2d::Build2DFragmentPreamble(&inner,
                                                        true,
                                                        use_array,
                                                        SamplerSlot::BaseColor,
                                                        use_array);

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &inner);
    vertices.push_back({VAT_VEC2, VAN::TexCoord});

    UBOSemanticSet ubos;
    SSBOSemanticSet ssbos;
    StaticTextureSamplerDescriptors samplers;
    AddTextureSampler(samplers, SamplerSlot::BaseColor, use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D);

    if(use_array)
        AddSSBODescriptor(ssbos, SSBODescriptorSemantic::MaterialBindingInstanceTexture);

    StaticMaterialDef def{};
    build2d::BuildBase2DFixedDef(def,
                                 "PureTexture2D",
                                 &inner,
                                 vertices,
                                 ubos,
                                 ssbos,
                                 &samplers,
                                 use_array ? ShaderDataSchema::TextureArrayID : ShaderDataSchema::None);

    return CreateFromFixedDef2D("PureTexture2D", profile, def, key, vs_preamble, fs_preamble, &inner, true);
}

MaterialCreateInfo *CreatePureTexture2D(const contract::PhysicalDeviceProfileLite *profile,
                                         const mtl::Material2DCreateConfig *cfg,
                                         MaterialVariantKey key)
{
    if(cfg && cfg->texture_source_bits_override != 0)
    {
        key.texture_source_bits = cfg->texture_source_bits_override;

        if (cfg->sampler_feature_bits_override != 0)
            key.sampler_feature_bits = cfg->sampler_feature_bits_override;
        else
            key.sampler_feature_bits = SamplerFeatureBit(SamplerSlot::BaseColor);
    }
    else if(cfg && cfg->sampler_feature_bits_override != 0)
    {
        key.sampler_feature_bits = cfg->sampler_feature_bits_override;
    }

    return CreatePureTextureVariant(profile, key, cfg);
}
}//namespace hgl::graph::mtl

