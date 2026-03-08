#pragma once

#include <hgl/ecs/core/System.h>
#include <hgl/ecs/core/AssetWorldRegistry.h>
#include <hgl/ecs/core/AssetTypes.h>

#include <vector>
#include <memory>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace hgl::ecs
{
    class ECSContext;
    class AssetInstanceComponent;

    class AssetInstanceBridgeSystem : public System
    {
    public:
        struct DrawPacket
        {
            InstanceId instance_id = 0;
            AssetWorldId asset_world_id = 0;
            AssetVersion resolved_version = 0;
            uint64_t proxy_handle = 0;
            uint16_t render_pass_id = 0;
            uint64_t material_bucket_key = 0;
        };

        struct RuntimeState
        {
            InstanceId instance_id = 0;
            AssetWorldId asset_world_id = 0;
            AssetVersion resolved_version = 0;
            uint64_t proxy_handle = 0;
            uint32_t rebuild_generation = 0;
            uint32_t last_observed_change_mask = 0;
            uint32_t last_update_frame_index = 0;
        };

        struct Stats
        {
            uint32_t instance_count = 0;
            uint32_t invalid_instance_id_count_frame = 0;
            uint32_t unresolved_count = 0;
            uint32_t dirty_count = 0;
            uint32_t rebuild_count_frame = 0;
            uint32_t runtime_state_count = 0;
            uint32_t runtime_state_peak_count = 0;
            uint32_t reclaimed_state_count_frame = 0;
            uint32_t version_refresh_count_frame = 0;
            uint32_t emitted_draw_packet_count_frame = 0;
            uint32_t sorted_draw_packet_count_frame = 0;
            uint32_t order_matches_previous_frame = 0;
            uint32_t stable_order_match_count_total = 0;
            uint32_t order_changed_count_total = 0;
            uint32_t emitted_bucket_count_frame = 0;
            uint32_t emitted_secondary_bucket_count_frame = 0;
        };

    private:
        ECSContext* world = nullptr;
        IAssetWorldRegistry* registry = nullptr;
        uint32_t rebuild_budget = 256;
        Stats stats{};
        uint64_t next_proxy_handle = 1;
        std::vector<std::weak_ptr<AssetInstanceComponent>> pending_rebuild;
        std::unordered_map<InstanceId, RuntimeState> runtime_states;
        std::unordered_set<AssetWorldId> pending_refresh_assets;
        std::vector<DrawPacket> draw_packets;
        std::unordered_map<AssetWorldId, uint32_t> draw_packet_bucket_counts;
        std::unordered_map<uint64_t, uint32_t> draw_packet_secondary_bucket_counts;
        uint64_t last_draw_order_hash = 0;
        bool has_last_draw_order_hash = false;

    public:
        explicit AssetInstanceBridgeSystem(const std::string& name = "AssetInstanceBridgeSystem");

        void SetWorld(ECSContext* w) { world = w; }
        void SetRegistry(IAssetWorldRegistry* value) { registry = value; }
        IAssetWorldRegistry* GetRegistry() const { return registry; }

        void SetRebuildBudget(uint32_t value) { rebuild_budget = value == 0 ? 1 : value; }
        uint32_t GetRebuildBudget() const { return rebuild_budget; }

        const Stats& GetStats() const { return stats; }
        size_t GetRuntimeStateCount() const { return runtime_states.size(); }
        const RuntimeState* FindRuntimeState(InstanceId id) const;
        const std::vector<DrawPacket>& GetDrawPackets() const { return draw_packets; }
        uint32_t GetDrawPacketCountForAsset(AssetWorldId id) const;
        uint32_t GetDrawPacketCountForSecondaryBucket(uint16_t render_pass_id, uint64_t material_bucket_key) const;
        void SetDiagnosticsLogEnabled(bool enabled) { diagnostics_log_enabled = enabled; }
        bool IsDiagnosticsLogEnabled() const { return diagnostics_log_enabled; }
        std::string BuildStatsReport() const;

        void Initialize() override;
        void Update(float deltaTime) override;

        void OnAssetWorldUpdated(AssetWorldId id, AssetVersion version);
        void OnAssetWorldEvicted(AssetWorldId id);

        void Collect(float deltaTime);
        void Rebuild(float deltaTime, uint32_t max_instances_per_frame);
        void SyncRender(float deltaTime);

    private:
        bool diagnostics_log_enabled = false;
    };
}
