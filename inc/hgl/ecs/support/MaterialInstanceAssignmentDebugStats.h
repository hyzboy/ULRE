#pragma once

#include <cstddef>
#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class MaterialBindingInstance;
    }

    namespace ecs
    {
        class MaterialResolveDiagnostics;
        class RenderItem;

        struct MaterialInstanceAssignmentDebugStats
        {
            uint32_t items_seen = 0;
            uint32_t null_item = 0;
            uint32_t triad_present = 0;
            uint32_t triad_only_candidate = 0;
            uint32_t triad_bridge_available = 0;
            uint32_t triad_bridge_mismatch = 0;
            uint32_t triad_bridge_rescue_candidate = 0;
            uint32_t triad_bridge_override_candidate = 0;
            uint32_t triad_bridge_compare_total = 0;
            uint32_t triad_bridge_compare_equal = 0;
            uint32_t shadow_switch_eval_total = 0;
            uint32_t shadow_switch_would_change = 0;
            uint32_t null_binding_instance = 0;
            uint32_t binding_id_mismatch = 0;
            uint32_t program_binding_program_mismatch = 0;
            uint32_t payload_binding_payload_mismatch = 0;
            uint32_t payload_id_mismatch = 0;
            uint32_t legacy_mi_program_mismatch = 0;

            bool HasAnyMismatch() const;
        };

        bool IsMaterialInstanceShadowBridgePreferEnabled(const MaterialResolveDiagnostics *diag);

        graph::MaterialBindingInstance *ResolveMaterialInstanceFromStateWithDebugStats(
            const RenderItem *item,
            const MaterialResolveDiagnostics *diag,
            MaterialInstanceAssignmentDebugStats *stats = nullptr);

        void LogMaterialInstanceAssignmentDebugSummary(const char *scope,
                                                      const MaterialResolveDiagnostics *diag,
                                                      const MaterialInstanceAssignmentDebugStats &stats,
                                                      size_t item_count,
                                                      size_t unique_mi_count);
    }//namespace ecs
}//namespace hgl
