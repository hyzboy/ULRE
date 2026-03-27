#include"Build2DCommon.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/MaterialCompiler.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/mtl/SamplerName.h>
#include<hgl/math/Vector.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialVariantDesc.h>
#include<cstdio>

namespace hgl::graph::mtl{

MaterialCreateInfo *CreatePureTextureVariant(const contract::PhysicalDeviceProfileLite *profile,
                                             const MaterialVariantKey &key,
                                             const mtl::Material2DCreateConfig *cfg)
{
    if(!profile||!cfg)
        return(nullptr);

    const bool has_slot_mode = key.HasAnyTextureSourceBits();
    const TextureSourceMode mode = has_slot_mode
        ? key.GetTextureSourceMode(SamplerSlot::BaseColor)
        : key.texture_source_mode;
    const bool use_array = (mode == TextureSourceMode::Array);

    const MaterialVariantDesc *var_desc = GetBuiltinVariantRegistry().QueryVariantWithCanonicalFallback(key,nullptr);
    if (!var_desc)
    {
        std::fprintf(stderr, "[PureTexture2D] VariantRegistry lookup failed\n");
        return nullptr;
    }

    // Array mode needs MI data for MaterialInstanceTextureID SSBO.
    constexpr const char mi_codes[] = "uvec4 id;";
    constexpr const uint32_t mi_bytes = sizeof(math::Vector4u);

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

    CompositorAssembler assembler;
    const auto result = assembler.Assemble(key, *var_desc);
    if (!result.success)
    {
        std::fprintf(stderr, "[PureTexture2D] CompositorAssembler failed: %s\n", result.error_message.c_str());
        return nullptr;
    }

    std::vector<FixedVertexEntry> vertices;
    build2d::PushBaseVertexEntries(vertices, &inner);
    vertices.push_back({VAT_VEC2, VertexInputRate::Vertex, VAN::TexCoord});

    FixedUBODescriptors ubos;
    FixedSSBODescriptors ssbos;
    FixedTextureSamplerDescriptors samplers;
    build2d::PushBaseUBODescriptors(ubos, &inner);
    build2d::PushBaseSSBODescriptors(ssbos, &inner);
    AddFixedTextureSampler(samplers,
                           SamplerSlot::BaseColor,
                           uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                           use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D);

    if(use_array)
        AddFixedSSBODescriptor(ssbos, SSBODescriptorSemantic::MaterialInstanceTextureID, uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT));

    FixedMaterialDef def {
        "PureTexture2D",
        inner.prim,
        vertices.data(), uint32_t(vertices.size()),
        &ubos,
        &ssbos,
        &samplers,
        use_array ? mi_codes : nullptr,
        use_array ? mi_bytes : 0,
    };

    std::string vs = vs_preamble + result.vertex_glsl;
    std::string fs = fs_preamble + result.fragment_glsl;

    MaterialCreateInfo *mci = CompileCompositorMaterial(profile, def, vs, fs, &inner);
    if(!mci)
        std::fprintf(stderr, "[PureTexture2D] CompileCompositorMaterial failed\n");
    return mci;
}

MaterialCreateInfo *CreatePureTexture2D(const contract::PhysicalDeviceProfileLite *profile,const mtl::Material2DCreateConfig *cfg)
{
    MaterialVariantKey key = MapPresetToVariantKey(MaterialPreset::PureTexture2D);

    if(cfg && cfg->texture_source_bits_override != 0)
    {
        key.texture_source_bits = cfg->texture_source_bits_override;

        key.texture_source_mode = TextureSourceMode::None;
        for (uint8_t s = 0; s < uint8_t(SamplerSlot::RANGE_SIZE); ++s)
        {
            const TextureSourceMode m = TextureSourceMode((key.texture_source_bits >> (uint32_t(s) * MaterialVariantKey::TextureSourceBitsPerSlot))
                                      & MaterialVariantKey::TextureSourceMask);
            if (m != TextureSourceMode::None) { key.texture_source_mode = m; break; }
        }

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
