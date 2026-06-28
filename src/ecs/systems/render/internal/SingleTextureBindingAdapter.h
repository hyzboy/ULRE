#pragma once

#include <hgl/ecs/core/EntityHandle.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/type/String.h>

#include <cstdint>
#include <string>

namespace hgl::ecs::internal
{
    enum class SingleTextureFallbackReason : uint8_t
    {
        None = 0,
        LegacyQuadPath,
        NonDomainCompatibility,
        MissingInput,
        MissingResource,
        InvalidRequest,
    };

    struct SingleTextureBindingRequest
    {
        EntityID entity_id = EntityID::Invalid();
        graph::mtl::SamplerSlot slot = graph::mtl::SamplerSlot::BaseColor;
        graph::mtl::TextureSourceMode source_mode = graph::mtl::TextureSourceMode::Simple;
        graph::TextureChannelHint channel_hint = graph::TextureChannelHint::RGBA;
        hgl::OSString texture_path;
        std::string domain_tag;

        bool HasDomainTag() const { return !domain_tag.empty(); }
    };

    struct SingleTextureBindingResult
    {
        bool success = false;
        bool used_fallback = false;
        SingleTextureFallbackReason fallback_reason = SingleTextureFallbackReason::None;
    };

    struct SingleTextureBindingStatsSnapshot
    {
        uint32_t main_path_hits = 0;
        uint32_t fallback_hits = 0;
        uint32_t reject_hits = 0;

        uint32_t fallback_legacy_quad_hits = 0;
        uint32_t fallback_nondomain_hits = 0;
        uint32_t reject_missing_input_hits = 0;
        uint32_t reject_missing_resource_hits = 0;
        uint32_t reject_invalid_request_hits = 0;
    };

    class SingleTextureBindingStats
    {
    public:

        static void RecordMainPathHit();
        static void RecordFallbackHit(SingleTextureFallbackReason reason);
        static void RecordRejectHit(SingleTextureFallbackReason reason);

        // Returns true only when at least one second has elapsed and a snapshot is emitted.
        static bool TryConsumePerSecondSnapshot(SingleTextureBindingStatsSnapshot &out_snapshot);

        static const char *ToString(SingleTextureFallbackReason reason);
    };
}
