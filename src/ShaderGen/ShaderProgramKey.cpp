#include <hgl/mtl/ShaderProgramKey.h>

namespace hgl::graph::mtl
{
    uint64 VertexProgramKey::Hash() const noexcept
    {
        uint64 h = hgl::hash::FNV1aInit<uint64>();
        h = hgl::hash::FNV1aAppend(h, position_provider);
        h = hgl::hash::FNV1aAppend(h, vertex_attribute_feature_bits);
        h = hgl::hash::FNV1aAppend(h, has_local_to_world ? uint8(1) : uint8(0));
        return h;
    }

    uint64 FragmentProgramKey::Hash() const noexcept
    {
        uint64 h = hgl::hash::FNV1aInit<uint64>();
        h = hgl::hash::FNV1aAppend(h, surface_type);
        h = hgl::hash::FNV1aAppend(h, blend_mode);
        h = hgl::hash::FNV1aAppend(h, lighting_model);
        h = hgl::hash::FNV1aAppend(h, sky_ambient_model);
        h = hgl::hash::FNV1aAppend(h, texture_source_bits);
        h = hgl::hash::FNV1aAppend(h, sampler_feature_bits);
        h = hgl::hash::FNV1aAppend(h, extra_feature_bits);
        h = hgl::hash::FNV1aAppend(h, effective_feature_mask);
        return h;
    }

    VertexProgramKey BuildVertexProgramKey(const MaterialVariantKey &key,
                                           const bool has_local_to_world) noexcept
    {
        VertexProgramKey out{};
        out.position_provider = key.position_provider;
        out.vertex_attribute_feature_bits = key.vertex_attribute_feature_bits;
        out.has_local_to_world = has_local_to_world;
        return out;
    }

    FragmentProgramKey BuildFragmentProgramKey(const MaterialVariantKey &key) noexcept
    {
        FragmentProgramKey out{};
        out.surface_type = key.surface_type;
        out.blend_mode = key.blend_mode;
        out.lighting_model = key.lighting_model;
        out.sky_ambient_model = key.sky_ambient_model;
        out.texture_source_bits = key.texture_source_bits;
        out.sampler_feature_bits = key.sampler_feature_bits;
        out.extra_feature_bits = key.extra_feature_bits;
        out.effective_feature_mask = key.effective_feature_mask;
        return out;
    }
}
