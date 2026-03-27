#pragma once

#include<hgl/mtl/FixedVertexEntry.h>
#include<hgl/mtl/DescriptorBindingContract.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<map>
#include<set>

namespace hgl::graph::mtl{

using FixedUBODescriptors = std::set<UBODescriptorSemantic>;
using FixedSSBODescriptors = std::set<SSBODescriptorSemantic>;

struct FixedTextureSamplerDescriptor
{
    SamplerType sampler_type = SamplerType::Sampler2D;
    uint32_t atlas_cols = 0;
    uint32_t atlas_rows = 0;
    TextureChannelHint channel_hint = TextureChannelHint::RGBA;
};

using FixedTextureSamplerDescriptors = std::map<SamplerSlot, FixedTextureSamplerDescriptor>;

inline constexpr FixedTextureSamplerDescriptor MakeFixedTextureSamplerDescriptor(
    const SamplerType sampler_type,
    const uint32_t atlas_cols = 0,
    const uint32_t atlas_rows = 0,
    const TextureChannelHint channel_hint = TextureChannelHint::RGBA)
{
    return FixedTextureSamplerDescriptor{sampler_type, atlas_cols, atlas_rows, channel_hint};
}

inline void AddFixedUBODescriptor(FixedUBODescriptors &descriptors,
                                  const UBODescriptorSemantic semantic)
{
    descriptors.insert(semantic);
}

inline void AddFixedSSBODescriptor(FixedSSBODescriptors &descriptors,
                                   const SSBODescriptorSemantic semantic)
{
    descriptors.insert(semantic);
}

inline void AddFixedTextureSampler(FixedTextureSamplerDescriptors &descriptors,
                                   const SamplerSlot slot,
                                   const SamplerType sampler_type,
                                   const uint32_t atlas_cols = 0,
                                   const uint32_t atlas_rows = 0,
                                   const TextureChannelHint channel_hint = TextureChannelHint::RGBA)
{
    descriptors[slot] = MakeFixedTextureSamplerDescriptor(sampler_type,
                                                          atlas_cols,
                                                          atlas_rows,
                                                          channel_hint);
}

struct FixedMaterialDef
{
    const char *                name;

    PrimitiveType               primitive_type;

    const FixedVertexEntry *    vertex_entries;
    uint32_t                    vertex_entry_count;

    const FixedUBODescriptors *ubo_descriptors = nullptr;
    const FixedSSBODescriptors *ssbo_descriptors = nullptr;
    const FixedTextureSamplerDescriptors *texture_samplers = nullptr;

    const char *                mi_glsl_codes;
    uint32_t                    mi_struct_bytes;
};

}//namespace hgl::graph::mtl
