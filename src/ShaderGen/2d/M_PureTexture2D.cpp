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

    std::vector<VertexAttributeSpec> specs;
    build2d::PushBaseVertexSpecs(specs, &inner);
    specs.push_back(MakeLegacyVertexAttributeSpec(VAT_VEC2, VAN::TexCoord));

    UBOSemanticSet ubos;
    SSBOSemanticSet ssbos;
    StaticTextureSamplerDescriptors samplers;
    AddTextureSampler(samplers, SamplerSlot::BaseColor, use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D);

    if(use_array)
        AddSSBODescriptor(ssbos, SSBODescriptorSemantic::MaterialInstanceTextureID);

    StaticMaterialDef def{};
    build2d::BuildBase2DSpecDef(def,
                                "PureTexture2D",
                                &inner,
                                specs,
                                ubos,
                                ssbos,
                                &samplers);
    // Array mode: keep legacy mi_glsl_codes/mi_struct_bytes to register MI data SSBO stride.
    // GLSL uses MATERIAL_INSTANCE_ID_ONLY so the data SSBO binding is declared but not read.
    if (use_array)
    {
        def.mi_glsl_codes  = mi_codes;
        def.mi_struct_bytes = mi_bytes;
    }

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

