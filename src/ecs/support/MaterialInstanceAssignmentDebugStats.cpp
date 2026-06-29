#include <hgl/ecs/support/MaterialInstanceAssignmentDebugStats.h>
#include <hgl/ecs/core/MaterialResolveDiagnostics.h>
#include <hgl/ecs/core/RenderItem.h>
#include <hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include <hgl/graph/module/MaterialDecoupledTypes.h>

#include <iostream>

namespace hgl::ecs
{
    bool MaterialInstanceAssignmentDebugStats::HasAnyMismatch() const
    {
        return null_item > 0
            || triad_only_candidate > 0
            || triad_bridge_mismatch > 0
            || triad_bridge_rescue_candidate > 0
            || triad_bridge_override_candidate > 0
            || (triad_bridge_compare_total > 0 && triad_bridge_compare_equal < triad_bridge_compare_total)
            || shadow_switch_would_change > 0
            || null_binding_instance > 0
            || binding_id_mismatch > 0
            || program_binding_program_mismatch > 0
            || payload_binding_payload_mismatch > 0
            || payload_id_mismatch > 0
            || legacy_mi_program_mismatch > 0;
    }

    bool IsMaterialInstanceShadowBridgePreferEnabled(const MaterialResolveDiagnostics *diag)
    {
    #ifdef _DEBUG
        return diag ? diag->IsR3ShadowBridgePreferEnabled() : false;
    #else
        (void)diag;
        return false;
    #endif
    }

    graph::MaterialBindingInstance *ResolveMaterialInstanceFromStateWithDebugStats(
        const RenderItem *item,
        const MaterialResolveDiagnostics *diag,
        MaterialInstanceAssignmentDebugStats *stats)
    {
        if (!item)
        {
            if (stats)
                ++stats->null_item;
            return nullptr;
        }

    #ifndef _DEBUG
        (void)diag;
        (void)stats;
        return item->GetResolvedMaterialState().binding_instance;
    #else
        if (stats)
            ++stats->items_seen;

        const auto state = item->GetResolvedMaterialState();
        auto *legacy_mi = state.binding_instance;
        auto *bridge_mi = state.program_binding ? state.program_binding->legacy_binding_instance : nullptr;

        if (stats && legacy_mi && bridge_mi)
        {
            ++stats->triad_bridge_compare_total;
            if (legacy_mi == bridge_mi)
                ++stats->triad_bridge_compare_equal;
        }

        if (stats && bridge_mi)
        {
            ++stats->shadow_switch_eval_total;
            if (bridge_mi != legacy_mi)
                ++stats->shadow_switch_would_change;
        }

        if (state.program_binding)
        {
            if (stats)
                ++stats->triad_present;

            if (state.program_binding->legacy_binding_instance)
            {
                if (stats)
                    ++stats->triad_bridge_available;
            }

            if (!state.binding_instance)
            {
                if (stats)
                    ++stats->triad_only_candidate;

                if (bridge_mi)
                {
                    if (stats)
                        ++stats->triad_bridge_rescue_candidate;
                }
            }
            else if (state.program_binding->legacy_binding_instance
                  && state.binding_instance != state.program_binding->legacy_binding_instance)
            {
                if (stats)
                    ++stats->triad_bridge_mismatch;

                if (stats)
                    ++stats->triad_bridge_override_candidate;
            }

            if (state.binding_id != 0 && state.binding_id != state.program_binding->id)
            {
                if (stats)
                    ++stats->binding_id_mismatch;
            }

            if (state.program && state.program_binding->program && state.program != state.program_binding->program)
            {
                if (stats)
                    ++stats->program_binding_program_mismatch;
            }

            if (state.payload && state.program_binding->payload && state.payload != state.program_binding->payload)
            {
                if (stats)
                    ++stats->payload_binding_payload_mismatch;
            }
        }

        if (state.payload && state.payload_id != 0 && state.payload->id != state.payload_id)
        {
            if (stats)
                ++stats->payload_id_mismatch;
        }

        if (!legacy_mi)
        {
            if (stats)
                ++stats->null_binding_instance;
        }
        else
        {
            auto *mi_program = hgl::graph::MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(legacy_mi);
            if (state.program && mi_program && state.program != mi_program)
            {
                if (stats)
                    ++stats->legacy_mi_program_mismatch;
            }
        }

        if (IsMaterialInstanceShadowBridgePreferEnabled(diag) && bridge_mi)
            return bridge_mi;

        return legacy_mi;
    #endif
    }

    void LogMaterialInstanceAssignmentDebugSummary(const char *scope,
                                                   const MaterialResolveDiagnostics *diag,
                                                   const MaterialInstanceAssignmentDebugStats &stats,
                                                   size_t item_count,
                                                   size_t unique_mi_count)
    {
    #ifndef _DEBUG
        (void)scope;
        (void)diag;
        (void)stats;
        (void)item_count;
        (void)unique_mi_count;
        return;
    #else
        if (!stats.HasAnyMismatch())
            return;

        static uint64_t summary_seq = 0;
        ++summary_seq;

        const bool should_log = summary_seq <= 8 || (summary_seq & (summary_seq - 1)) == 0;
        if (!should_log)
            return;

        const uint32_t compare_total = stats.triad_bridge_compare_total;
        const uint32_t compare_equal = stats.triad_bridge_compare_equal;
        const uint32_t compare_diff = compare_total >= compare_equal ? (compare_total - compare_equal) : 0;
        const uint32_t diff_rate_percent = compare_total > 0
                                         ? static_cast<uint32_t>((static_cast<uint64_t>(compare_diff) * 100ull) / compare_total)
                                         : 0u;
        const uint32_t shadow_eval_total = stats.shadow_switch_eval_total;
        const uint32_t shadow_would_change = stats.shadow_switch_would_change;
        const uint32_t shadow_change_rate_percent = shadow_eval_total > 0
                                                  ? static_cast<uint32_t>((static_cast<uint64_t>(shadow_would_change) * 100ull) / shadow_eval_total)
                                                  : 0u;

        std::cout << "[MaterialInstanceAssignmentBuffer] DEBUG[" << (scope ? scope : "unknown") << "] "
                  << "seq=" << summary_seq
                  << "items=" << item_count
                  << " unique_mi=" << unique_mi_count
                  << " seen=" << stats.items_seen
                  << " null_item=" << stats.null_item
                  << " triad_present=" << stats.triad_present
                  << " triad_only_candidate=" << stats.triad_only_candidate
                  << " triad_bridge_available=" << stats.triad_bridge_available
                  << " triad_bridge_mismatch=" << stats.triad_bridge_mismatch
                  << " triad_bridge_rescue_candidate=" << stats.triad_bridge_rescue_candidate
                  << " triad_bridge_override_candidate=" << stats.triad_bridge_override_candidate
                  << " triad_bridge_compare_total=" << compare_total
                  << " triad_bridge_compare_equal=" << compare_equal
                  << " triad_bridge_compare_diff=" << compare_diff
                  << " triad_bridge_compare_diff_rate=" << diff_rate_percent << "%"
                  << " shadow_switch_enabled=" << (IsMaterialInstanceShadowBridgePreferEnabled(diag) ? 1 : 0)
                  << " shadow_switch_eval_total=" << shadow_eval_total
                  << " shadow_switch_would_change=" << shadow_would_change
                  << " shadow_switch_change_rate=" << shadow_change_rate_percent << "%"
                  << " null_binding_instance=" << stats.null_binding_instance
                  << " binding_id_mismatch=" << stats.binding_id_mismatch
                  << " binding_program_mismatch=" << stats.program_binding_program_mismatch
                  << " binding_payload_mismatch=" << stats.payload_binding_payload_mismatch
                  << " payload_id_mismatch=" << stats.payload_id_mismatch
                  << " legacy_mi_program_mismatch=" << stats.legacy_mi_program_mismatch
                  << std::endl;
    #endif
    }
}
