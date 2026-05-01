#include <hgl/mtl/MaterialKey.h>

namespace hgl::graph::mtl {

    // ---------------------------------------------------------------------------
    // Hash
    // Seed from the variant's own hash, then fold in all MaterialKey-specific
    // fields.  _reserved is intentionally excluded so that callers may use it
    // as a scratch field without affecting identity.
    // ---------------------------------------------------------------------------
    uint64 MaterialKey::Hash() const noexcept
    {
        uint64 h = variant.Hash();
        h = hgl::hash::FNV1aAppend(h, pass);
        h = hgl::hash::FNV1aAppend(h, def_id);
        h = hgl::hash::FNV1aAppend(h, schema);
        h = hgl::hash::FNV1aAppend(h, glsl_version);
        h = hgl::hash::FNV1aAppend(h, vk_version);
        h = hgl::hash::FNV1aAppend(h, spv_version);
        // _reserved intentionally excluded
        return h;
    }

    // ---------------------------------------------------------------------------
    // operator<=>
    // MaterialVariantKey has no spaceship operator, so we compare its fields
    // one-by-one, then continue with the MaterialKey-specific fields.
    // Field order mirrors the struct layout to keep cache locality predictable.
    // ---------------------------------------------------------------------------
    std::strong_ordering MaterialKey::operator<=>(const MaterialKey &rhs) const noexcept
    {
        // --- MaterialVariantKey fields ---
        if (auto c = static_cast<uint8>(variant.surface_type)
                 <=> static_cast<uint8>(rhs.variant.surface_type); c != 0) return c;

        if (auto c = static_cast<uint8>(variant.geometry_mode)
                 <=> static_cast<uint8>(rhs.variant.geometry_mode); c != 0) return c;

        if (auto c = variant.texture_source_bits
                 <=> rhs.variant.texture_source_bits; c != 0) return c;

        if (auto c = variant.sampler_feature_bits
                 <=> rhs.variant.sampler_feature_bits; c != 0) return c;

        if (auto c = variant.vertex_attribute_feature_bits
                 <=> rhs.variant.vertex_attribute_feature_bits; c != 0) return c;

        if (auto c = variant.extra_feature_bits
                 <=> rhs.variant.extra_feature_bits; c != 0) return c;

        if (auto c = static_cast<uint8>(variant.blend_mode)
                 <=> static_cast<uint8>(rhs.variant.blend_mode); c != 0) return c;

        if (auto c = static_cast<uint8>(variant.pass_hint)
                 <=> static_cast<uint8>(rhs.variant.pass_hint); c != 0) return c;

        if (auto c = static_cast<uint8>(variant.sky_ambient_model)
                 <=> static_cast<uint8>(rhs.variant.sky_ambient_model); c != 0) return c;

        if (auto c = static_cast<uint8>(variant.lighting_model)
                 <=> static_cast<uint8>(rhs.variant.lighting_model); c != 0) return c;

        if (auto c = variant.effective_feature_mask
                 <=> rhs.variant.effective_feature_mask; c != 0) return c;

        // Phase B: attribute_providers comparison (lexicographic over the array).
        for (size_t i = 0; i < variant.attribute_providers.size(); ++i)
        {
            if (auto c = static_cast<uint16>(variant.attribute_providers[i])
                     <=> static_cast<uint16>(rhs.variant.attribute_providers[i]); c != 0)
                return c;
        }

        // --- MaterialKey-specific fields ---
        if (auto c = static_cast<uint8>(pass)
                 <=> static_cast<uint8>(rhs.pass); c != 0) return c;

        if (auto c = def_id <=> rhs.def_id; c != 0) return c;

        if (auto c = static_cast<uint32>(schema)
                 <=> static_cast<uint32>(rhs.schema); c != 0) return c;

        if (auto c = glsl_version <=> rhs.glsl_version; c != 0) return c;

        if (auto c = vk_version <=> rhs.vk_version; c != 0) return c;

        return spv_version <=> rhs.spv_version;
        // _reserved excluded from ordering, same as Hash()
    }

} // namespace hgl::graph::mtl
