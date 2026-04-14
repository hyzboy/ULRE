#pragma once

#include <hgl/common/PrimitiveTypeDef.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>

#include <string>
#include <cstdint>

namespace hgl::graph
{

class Geometry;

using SemanticMaterialId = uint64_t;

struct RuntimeMaterialRequest
{
    // Final render pipeline preset resolved at runtime (pass/view dependent).
    GraphicsPipelinePreset pipeline = GraphicsPipelinePreset::Solid3D;

    // Runtime batching/domain partition key. Empty means default domain.
    std::string domain_id;

    // Runtime policy bits reserved for future expansion.
    // bit0: auto transparency enabled
    // bit1: dither lod enabled
    uint32_t policy_flags = 0;

    // Runtime-selected material behavior knobs.
    uint8_t transparency_mode = 0; // 0=opaque,1=alpha_test,2=dither,3=blend
    uint8_t lod_tier = 0;          // runtime selected lod tier

    bool operator==(const RuntimeMaterialRequest &o) const
    {
        return pipeline == o.pipeline
            && domain_id == o.domain_id
            && policy_flags == o.policy_flags
            && transparency_mode == o.transparency_mode
            && lod_tier == o.lod_tier;
    }
};

struct GeometrySignature
{
    PrimitiveType primitive = PrimitiveType::Triangles;

    // FNV-1a hash of the bound VIL's VertexInputFormat list (format + location).
    // Zero when VIL has not been derived yet (deferred path).
    uint32_t vil_hash = 0;

    // Phase 3: strong layout discriminator — FNV-1a hash of each VAB's
    // (format, stride) pair in binding order.  Differentiates deferred
    // primitives whose VIL has not been created yet (vil_hash == 0) but
    // whose geometry layouts are distinct.
    uint32_t geometry_layout_hash = 0;

    // Runtime-only source for VIL auto-derivation.  Intentionally excluded
    // from operator== so VariantKey remains stable across pointer changes.
    const Geometry *geometry_for_vil_derivation = nullptr;

    bool operator==(const GeometrySignature &o) const
    {
        if (primitive != o.primitive || vil_hash != o.vil_hash)
            return false;
        // geometry_layout_hash is only a discriminator for the deferred path
        // (vil_hash == 0).  When a VIL has been resolved the material-required
        // attribs are fully described by vil_hash; including geometry_layout_hash
        // would prevent primitives that share the same material VIL but differ
        // in extra unused geometry attributes from sharing a variant cache entry.
        if (vil_hash == 0 && geometry_layout_hash != o.geometry_layout_hash)
            return false;
        return true;
    }
};

} // namespace hgl::graph
