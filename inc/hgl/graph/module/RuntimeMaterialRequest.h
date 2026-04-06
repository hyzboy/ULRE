#pragma once

#include <hgl/common/PrimitiveTypeDef.h>
#include <hgl/vk/pipeline/VKGraphicsPipelinePreset.h>

#include <string>
#include <cstdint>

namespace hgl::graph
{

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

    // Optional future extension points for VIL/vertex layout signatures.
    uint32_t vil_hash = 0;

    bool operator==(const GeometrySignature &o) const
    {
        return primitive == o.primitive
            && vil_hash == o.vil_hash;
    }
};

} // namespace hgl::graph
