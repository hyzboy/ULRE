#pragma once

#include<hgl/mtl/FixedVertexEntry.h>
#include<hgl/mtl/DescriptorBindingContract.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<map>
#include<string>

namespace hgl::graph::mtl{

using FixedUBODescriptors = std::map<UBODescriptorSemantic, uint32_t>;
using FixedSSBODescriptors = std::map<SSBODescriptorSemantic, uint32_t>;

struct FixedTextureSamplerDescriptor
{
    DescriptorSetType set_type = SET_TYPE_MATERIAL;
    uint32_t stage_flags = 0;
    SamplerType sampler_type = SamplerType::Sampler2D;
    uint32_t atlas_cols = 0;
    uint32_t atlas_rows = 0;
};

struct FixedTextureSamplerDescriptors
{
    std::map<SamplerSlot, FixedTextureSamplerDescriptor> by_slot;
    std::map<std::string, FixedTextureSamplerDescriptor> by_name;
};

inline constexpr FixedTextureSamplerDescriptor MakeFixedTextureSamplerDescriptor(
    const uint32_t stage_flags,
    const SamplerType sampler_type,
    const DescriptorSetType set_type = SET_TYPE_MATERIAL,
    const uint32_t atlas_cols = 0,
    const uint32_t atlas_rows = 0)
{
    return FixedTextureSamplerDescriptor{set_type, stage_flags, sampler_type, atlas_cols, atlas_rows};
}

inline void AddFixedUBODescriptor(FixedUBODescriptors &descriptors,
                                  const UBODescriptorSemantic semantic,
                                  const uint32_t stage_flags)
{
    descriptors[semantic] |= stage_flags;
}

inline void AddFixedSSBODescriptor(FixedSSBODescriptors &descriptors,
                                   const SSBODescriptorSemantic semantic,
                                   const uint32_t stage_flags)
{
    descriptors[semantic] |= stage_flags;
}

inline void AddFixedTextureSampler(FixedTextureSamplerDescriptors &descriptors,
                                   const SamplerSlot slot,
                                   const uint32_t stage_flags,
                                   const SamplerType sampler_type,
                                   const DescriptorSetType set_type = SET_TYPE_MATERIAL,
                                   const uint32_t atlas_cols = 0,
                                   const uint32_t atlas_rows = 0)
{
    descriptors.by_slot[slot] = MakeFixedTextureSamplerDescriptor(stage_flags, sampler_type, set_type, atlas_cols, atlas_rows);
}

inline void AddFixedNamedTextureSampler(FixedTextureSamplerDescriptors &descriptors,
                                        const char *name,
                                        const uint32_t stage_flags,
                                        const SamplerType sampler_type,
                                        const DescriptorSetType set_type,
                                        const uint32_t atlas_cols = 0,
                                        const uint32_t atlas_rows = 0)
{
    if (!name || !*name)
        return;

    descriptors.by_name[name] = MakeFixedTextureSamplerDescriptor(stage_flags, sampler_type, set_type, atlas_cols, atlas_rows);
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
