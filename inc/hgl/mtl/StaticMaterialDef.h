#pragma once

#include<hgl/mtl/FixedVertexEntry.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>
#include<hgl/mtl/ShaderDataSchema.h>
#include<hgl/vk/VKPrimitiveType.h>
#include<map>
#include<set>

namespace hgl::graph::mtl{

using UBOSemanticSet = std::set<UBODescriptorSemantic>;
using SSBOSemanticSet = std::set<SSBODescriptorSemantic>;

struct StaticTextureSamplerDescriptor
{
    SamplerType sampler_type = SamplerType::Sampler2D;
    uint32_t atlas_cols = 0;
    uint32_t atlas_rows = 0;
    TextureChannelHint channel_hint = TextureChannelHint::RGBA;
};

using StaticTextureSamplerDescriptors = std::map<SamplerSlot, StaticTextureSamplerDescriptor>;

inline constexpr StaticTextureSamplerDescriptor MakeStaticTextureSamplerDescriptor(
    const SamplerType sampler_type,
    const uint32_t atlas_cols = 0,
    const uint32_t atlas_rows = 0,
    const TextureChannelHint channel_hint = TextureChannelHint::RGBA)
{
    return StaticTextureSamplerDescriptor{sampler_type, atlas_cols, atlas_rows, channel_hint};
}

inline void AddUBODescriptor(UBOSemanticSet &descriptors,
                                  const UBODescriptorSemantic semantic)
{
    descriptors.insert(semantic);
}

inline void AddSSBODescriptor(SSBOSemanticSet &descriptors,
                                   const SSBODescriptorSemantic semantic)
{
    descriptors.insert(semantic);
}

inline void AddTextureSampler(StaticTextureSamplerDescriptors &descriptors,
                                   const SamplerSlot slot,
                                   const SamplerType sampler_type,
                                   const uint32_t atlas_cols = 0,
                                   const uint32_t atlas_rows = 0,
                                   const TextureChannelHint channel_hint = TextureChannelHint::RGBA)
{
    descriptors[slot] = MakeStaticTextureSamplerDescriptor(sampler_type,
                                                          atlas_cols,
                                                          atlas_rows,
                                                          channel_hint);
}

struct StaticMaterialDef
{
    const char *                name;

    PrimitiveType               primitive_type;

    const FixedVertexEntry *    vertex_entries;
    uint32_t                    vertex_entry_count;

    // Borrowed pointers (non-owning).
    // Lifetime contract:
    // - If non-null, the pointed containers must outlive every consumer using this StaticMaterialDef.
    // - Static/global defs may point to static storage.
    // - Runtime merged defs may point to caller-owned temporary storage only within the call chain.
    const UBOSemanticSet *ubo_descriptors = nullptr;
    const SSBOSemanticSet *ssbo_descriptors = nullptr;
    const StaticTextureSamplerDescriptors *texture_samplers = nullptr;

    ShaderDataSchema            shader_data_schema = ShaderDataSchema::None;
};

struct StaticVertexDefView
{
    PrimitiveType primitive_type = PrimitiveType::Triangles;
    const FixedVertexEntry *vertex_entries = nullptr;
    uint32_t vertex_entry_count = 0;
};

struct StaticFragmentDefView
{
    const UBOSemanticSet *ubo_descriptors = nullptr;
    const SSBOSemanticSet *ssbo_descriptors = nullptr;
    const StaticTextureSamplerDescriptors *texture_samplers = nullptr;
    ShaderDataSchema shader_data_schema = ShaderDataSchema::None;
};

inline StaticVertexDefView BuildVertexDefFromStaticMaterialDef(const StaticMaterialDef &def) noexcept
{
    StaticVertexDefView view{};
    view.primitive_type = def.primitive_type;
    view.vertex_entries = def.vertex_entries;
    view.vertex_entry_count = def.vertex_entry_count;
    return view;
}

inline StaticFragmentDefView BuildFragmentDefFromStaticMaterialDef(const StaticMaterialDef &def) noexcept
{
    StaticFragmentDefView view{};
    view.ubo_descriptors = def.ubo_descriptors;
    view.ssbo_descriptors = def.ssbo_descriptors;
    view.texture_samplers = def.texture_samplers;
    view.shader_data_schema = def.shader_data_schema;
    return view;
}

}//namespace hgl::graph::mtl
