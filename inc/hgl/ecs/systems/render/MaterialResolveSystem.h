#pragma once

/// MaterialResolveSystem — 延迟 MI 解析 ECS System
///
/// 在 RenderMaterialBind Phase 执行，遍历所有 dirty MaterialResolveRequest 的
/// PrimitiveComponent，根据 record + Geometry GVF 自动解析 MI；
/// 若 PrimitiveComponent 仅持有 unresolved_geometry，则同时创建 Primitive。

#include<hgl/ecs/core/System.h>
#include<hgl/graph/module/MaterialResolveTieredCache.h>

#include <cstdint>
#include <deque>
#include <unordered_map>

namespace hgl::ecs
{
    class ECSContext;

    class MaterialResolveSystem : public System
    {
    private:

        ECSContext *world = nullptr;
        bool decoupled_cache_enabled = false;   // Phase R1.2 placeholder, default off.
        bool decoupled_cache_dryrun_short_circuit_check_enabled = true;
        uint64_t last_cache_stats_log_ms = 0;
        uint64_t cache_stats_log_interval_ms = 1000;

        // Phase R2.0: shadow ownership for payload/binding objects used only by tiered-cache probing.
        uint64_t next_shadow_payload_id = 1;
        uint64_t next_shadow_binding_id = 1;
        std::deque<graph::MaterialInstancePayload> shadow_payload_storage;
        std::deque<graph::ProgramInstanceBinding> shadow_binding_storage;
        std::unordered_map<graph::PayloadCacheKey,
                   graph::MaterialInstancePayload *,
                   graph::PayloadCacheKeyHash> shadow_payload_index;
        std::unordered_map<graph::BindingCacheKey,
                   graph::ProgramInstanceBinding *,
                   graph::BindingCacheKeyHash> shadow_binding_index;

        struct R21DryRunStats
        {
            uint64_t tasks_seen = 0;
            uint64_t candidate_program_hits = 0;
            uint64_t candidate_payload_hits = 0;
            uint64_t candidate_binding_hits = 0;

            uint64_t miss_program = 0;
            uint64_t miss_payload = 0;
            uint64_t miss_binding = 0;

            uint64_t program_match_with_legacy = 0;
            uint64_t payload_match_with_legacy = 0;
            uint64_t binding_match_with_legacy = 0;

            uint64_t short_circuit_checks = 0;
            uint64_t dry_run_short_circuit_eligible = 0;
            uint64_t short_circuit_blocked_by_program = 0;
            uint64_t short_circuit_blocked_by_payload = 0;
            uint64_t short_circuit_blocked_by_binding = 0;
        };

        R21DryRunStats r21_dry_run_stats{};

    public:

        MaterialResolveSystem(const std::string &name = "MaterialResolveSystem");
        ~MaterialResolveSystem() override = default;

        void SetWorld(ECSContext *w) { world = w; }
        void SetDecoupledCacheEnabled(bool enabled) { decoupled_cache_enabled = enabled; }
        bool IsDecoupledCacheEnabled() const { return decoupled_cache_enabled; }
        void SetDecoupledCacheDryRunShortCircuitCheckEnabled(bool enabled) { decoupled_cache_dryrun_short_circuit_check_enabled = enabled; }
        bool IsDecoupledCacheDryRunShortCircuitCheckEnabled() const { return decoupled_cache_dryrun_short_circuit_check_enabled; }

        void Update(float deltaTime) override;
    };
}//namespace hgl::ecs
