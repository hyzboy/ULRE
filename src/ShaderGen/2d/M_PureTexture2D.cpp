#include"Build2DCommon.h"
#include"MaterialFactory2D.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/mtl/SamplerSlot.h>
#include <memory>

namespace hgl::graph::mtl{

std::unique_ptr<MaterialCreateInfo> CreatePureTextureVariantOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                                  const MaterialVariantKey &key,
                                                                  const mtl::Material2DCreateConfig *cfg,
                                                                  const MaterialVariantDesc &desc)
{
    if(!profile||!cfg)
        return nullptr;

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

    MaterialResourceManifest manifest;
    AddTextureSampler(manifest.samplers, SamplerSlot::BaseColor, use_array ? SamplerType::Sampler2DArray : SamplerType::Sampler2D);

    if(use_array)
        AddSSBODescriptor(manifest.ssbos, SSBODescriptorSemantic::MaterialBindingInstanceTexture);

    StaticMaterialDef def{};
    build2d::BuildBase2DFixedDef(def,
                                 "PureTexture2D",
                                 &inner,
                                 vertices,
                                 manifest,
                                 use_array ? ShaderDataSchema::TextureArrayID : ShaderDataSchema::None);

    return CreateFromFixedDef2DOwned("PureTexture2D", profile, def, key, vs_preamble, fs_preamble, &inner, desc);
}

std::unique_ptr<MaterialCreateInfo> CreatePureTexture2DOwned(const contract::PhysicalDeviceProfileLite *profile,
                                                             const mtl::Material2DCreateConfig *cfg,
                                                             const MaterialVariantDesc &desc,
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

    return CreatePureTextureVariantOwned(profile, key, cfg, desc);
}

static std::unique_ptr<MaterialCreateInfo> PureTexture2D_Adapter(
    const contract::PhysicalDeviceProfileLite *profile,
    const MaterialVariantDesc                 *desc,
    const MaterialVariantKey                  &key,
    MaterialCreateConfig *cfg)
{ return CreatePureTexture2DOwned(profile, static_cast<const Material2DCreateConfig *>(cfg), *desc, key); }
}//namespace hgl::graph::mtl

#include "../MaterialFactory3DRegistration.h"
ULRE_REGISTER_PRESET_FACTORY(PureTexture2D, "PureTexture2D", hgl::graph::mtl::PureTexture2D_Adapter)

