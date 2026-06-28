#include "SingleTextureBindingAdapter.h"

#include <chrono>

namespace hgl::ecs::internal
{
    namespace
    {
        struct SingleTextureBindingStatsState
        {
            uint32_t main_path_hits = 0;
            uint32_t fallback_hits = 0;
            uint32_t reject_hits = 0;

            uint32_t fallback_legacy_quad_hits = 0;
            uint32_t fallback_nondomain_hits = 0;
            uint32_t reject_missing_input_hits = 0;
            uint32_t reject_missing_resource_hits = 0;
            uint32_t reject_invalid_request_hits = 0;

            uint64_t last_emit_ms = 0;
        };

        static SingleTextureBindingStatsState g_single_texture_binding_stats;

        static uint64_t GetNowMs()
        {
            using namespace std::chrono;
            return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        }

        static void ResetSnapshotCounters(SingleTextureBindingStatsState &stats)
        {
            stats.main_path_hits = 0;
            stats.fallback_hits = 0;
            stats.reject_hits = 0;

            stats.fallback_legacy_quad_hits = 0;
            stats.fallback_nondomain_hits = 0;
            stats.reject_missing_input_hits = 0;
            stats.reject_missing_resource_hits = 0;
            stats.reject_invalid_request_hits = 0;
        }
    }

    void SingleTextureBindingStats::RecordMainPathHit()
    {
        ++g_single_texture_binding_stats.main_path_hits;
    }

    void SingleTextureBindingStats::RecordFallbackHit(SingleTextureFallbackReason reason)
    {
        ++g_single_texture_binding_stats.fallback_hits;

        switch (reason)
        {
            case SingleTextureFallbackReason::LegacyQuadPath:
                ++g_single_texture_binding_stats.fallback_legacy_quad_hits;
                break;
            case SingleTextureFallbackReason::NonDomainCompatibility:
                ++g_single_texture_binding_stats.fallback_nondomain_hits;
                break;
            default:
                break;
        }
    }

    void SingleTextureBindingStats::RecordRejectHit(SingleTextureFallbackReason reason)
    {
        ++g_single_texture_binding_stats.reject_hits;

        switch (reason)
        {
            case SingleTextureFallbackReason::MissingInput:
                ++g_single_texture_binding_stats.reject_missing_input_hits;
                break;
            case SingleTextureFallbackReason::MissingResource:
                ++g_single_texture_binding_stats.reject_missing_resource_hits;
                break;
            case SingleTextureFallbackReason::InvalidRequest:
                ++g_single_texture_binding_stats.reject_invalid_request_hits;
                break;
            default:
                break;
        }
    }

    bool SingleTextureBindingStats::TryConsumePerSecondSnapshot(SingleTextureBindingStatsSnapshot &out_snapshot)
    {
        auto &stats = g_single_texture_binding_stats;
        const uint64_t now_ms = GetNowMs();

        if (stats.last_emit_ms == 0)
        {
            stats.last_emit_ms = now_ms;
            return false;
        }

        if (now_ms - stats.last_emit_ms < 1000)
            return false;

        if (stats.main_path_hits == 0 && stats.fallback_hits == 0 && stats.reject_hits == 0)
        {
            stats.last_emit_ms = now_ms;
            return false;
        }

        out_snapshot.main_path_hits = stats.main_path_hits;
        out_snapshot.fallback_hits = stats.fallback_hits;
        out_snapshot.reject_hits = stats.reject_hits;
        out_snapshot.fallback_legacy_quad_hits = stats.fallback_legacy_quad_hits;
        out_snapshot.fallback_nondomain_hits = stats.fallback_nondomain_hits;
        out_snapshot.reject_missing_input_hits = stats.reject_missing_input_hits;
        out_snapshot.reject_missing_resource_hits = stats.reject_missing_resource_hits;
        out_snapshot.reject_invalid_request_hits = stats.reject_invalid_request_hits;

        ResetSnapshotCounters(stats);
        stats.last_emit_ms = now_ms;
        return true;
    }

    const char *SingleTextureBindingStats::ToString(SingleTextureFallbackReason reason)
    {
        switch (reason)
        {
            case SingleTextureFallbackReason::None: return "none";
            case SingleTextureFallbackReason::LegacyQuadPath: return "legacy_quad_path";
            case SingleTextureFallbackReason::NonDomainCompatibility: return "nondomain_compatibility";
            case SingleTextureFallbackReason::MissingInput: return "missing_input";
            case SingleTextureFallbackReason::MissingResource: return "missing_resource";
            case SingleTextureFallbackReason::InvalidRequest: return "invalid_request";
            default: return "unknown";
        }
    }
}
