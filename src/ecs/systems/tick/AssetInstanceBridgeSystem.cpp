#include <hgl/ecs/systems/tick/AssetInstanceBridgeSystem.h>

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/AssetInstanceComponent.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace hgl::ecs
{
    namespace
    {
        uint64_t ComposeSecondaryBucketKey(const uint16_t render_pass_id, const uint64_t material_bucket_key)
        {
            return (static_cast<uint64_t>(render_pass_id) << 48) ^ (material_bucket_key & 0x0000FFFFFFFFFFFFull);
        }

        void HashCombine(uint64_t &seed, uint64_t value)
        {
            constexpr uint64_t kMul = 1099511628211ull;
            seed ^= value;
            seed *= kMul;
        }
    }

    uint32_t AssetInstanceBridgeSystem::GetDrawPacketCountForSecondaryBucket(uint16_t render_pass_id, uint64_t material_bucket_key) const
    {
        const uint64_t key = ComposeSecondaryBucketKey(render_pass_id, material_bucket_key);
        auto it = draw_packet_secondary_bucket_counts.find(key);
        if (it == draw_packet_secondary_bucket_counts.end())
            return 0;

        return it->second;
    }

    uint32_t AssetInstanceBridgeSystem::GetDrawPacketCountForAsset(AssetWorldId id) const
    {
        auto it = draw_packet_bucket_counts.find(id);
        if (it == draw_packet_bucket_counts.end())
            return 0;

        return it->second;
    }

    const AssetInstanceBridgeSystem::RuntimeState* AssetInstanceBridgeSystem::FindRuntimeState(InstanceId id) const
    {
        auto it = runtime_states.find(id);
        if (it == runtime_states.end())
            return nullptr;

        return &it->second;
    }

    AssetInstanceBridgeSystem::AssetInstanceBridgeSystem(const std::string& name)
        : System(name)
    {
        SetExecutionOrder(ExecutionPhase::TickTransform);
    }

    void AssetInstanceBridgeSystem::Initialize()
    {
        SetInitialized(true);
    }

    void AssetInstanceBridgeSystem::Update(float deltaTime)
    {
        Collect(deltaTime);
        Rebuild(deltaTime, rebuild_budget);
        SyncRender(deltaTime);

        if (diagnostics_log_enabled)
            std::cout << BuildStatsReport() << std::endl;
    }

    std::string AssetInstanceBridgeSystem::BuildStatsReport() const
    {
        std::ostringstream oss;
        oss << "[AssetInstanceBridgeSystem]"
            << " instance=" << stats.instance_count
            << " invalid=" << stats.invalid_instance_id_count_frame
            << " unresolved=" << stats.unresolved_count
            << " dirty=" << stats.dirty_count
            << " rebuild=" << stats.rebuild_count_frame
            << " runtime=" << stats.runtime_state_count
            << " runtime_peak=" << stats.runtime_state_peak_count
            << " reclaimed=" << stats.reclaimed_state_count_frame
            << " refresh=" << stats.version_refresh_count_frame
            << " packets=" << stats.emitted_draw_packet_count_frame
            << " sorted_packets=" << stats.sorted_draw_packet_count_frame
            << " buckets=" << stats.emitted_bucket_count_frame
            << " secondary_buckets=" << stats.emitted_secondary_bucket_count_frame
            << " order_match_prev=" << stats.order_matches_previous_frame
            << " stable_total=" << stats.stable_order_match_count_total
            << " changed_total=" << stats.order_changed_count_total;

        return oss.str();
    }

    void AssetInstanceBridgeSystem::OnAssetWorldUpdated(AssetWorldId id, AssetVersion)
    {
        if (id == 0)
            return;

        pending_refresh_assets.insert(id);
    }

    void AssetInstanceBridgeSystem::OnAssetWorldEvicted(AssetWorldId id)
    {
        if (id == 0)
            return;

        for (auto it = runtime_states.begin(); it != runtime_states.end();)
        {
            if (it->second.asset_world_id == id)
                it = runtime_states.erase(it);
            else
                ++it;
        }

        pending_refresh_assets.erase(id);
    }

    void AssetInstanceBridgeSystem::Collect(float)
    {
        stats.instance_count = 0;
        stats.invalid_instance_id_count_frame = 0;
        stats.unresolved_count = 0;
        stats.dirty_count = 0;
        stats.rebuild_count_frame = 0;
        stats.runtime_state_count = static_cast<uint32_t>(runtime_states.size());
        stats.reclaimed_state_count_frame = 0;
        stats.version_refresh_count_frame = 0;
        stats.emitted_draw_packet_count_frame = 0;
        stats.sorted_draw_packet_count_frame = 0;
        stats.order_matches_previous_frame = 0;
        stats.emitted_bucket_count_frame = 0;
        stats.emitted_secondary_bucket_count_frame = 0;
        pending_rebuild.clear();

        if (!world)
            return;

        std::vector<std::shared_ptr<AssetInstanceComponent>> components;
        world->GetComponents(components);
        std::unordered_set<InstanceId> live_instance_ids;
        live_instance_ids.reserve(components.size());

        stats.instance_count = static_cast<uint32_t>(components.size());

        for (const auto& comp : components)
        {
            if (!comp)
                continue;

            const InstanceId instance_id = comp->GetInstanceID();
            if (instance_id == 0)
            {
                ++stats.invalid_instance_id_count_frame;
                continue;
            }

            live_instance_ids.insert(instance_id);

            const bool resolved = registry && registry->Exists(comp->GetAssetWorldID());
            if (!resolved)
            {
                ++stats.unresolved_count;

                // Drop stale runtime state when asset definition is unavailable.
                if (runtime_states.erase(instance_id) > 0)
                    ++stats.reclaimed_state_count_frame;
            }

            const bool dirty = comp->GetChangeMask() != 0;
            const bool refresh_requested = pending_refresh_assets.find(comp->GetAssetWorldID()) != pending_refresh_assets.end();
            if (dirty)
                ++stats.dirty_count;

            if (refresh_requested && resolved)
                ++stats.version_refresh_count_frame;

            if ((dirty || refresh_requested) && resolved)
                pending_rebuild.push_back(comp);
        }

        // Drop states whose instance no longer exists in the world.
        for (auto it = runtime_states.begin(); it != runtime_states.end();)
        {
            if (live_instance_ids.find(it->first) == live_instance_ids.end())
            {
                it = runtime_states.erase(it);
                ++stats.reclaimed_state_count_frame;
            }
            else
            {
                ++it;
            }
        }

        stats.runtime_state_count = static_cast<uint32_t>(runtime_states.size());
        if (stats.runtime_state_count > stats.runtime_state_peak_count)
            stats.runtime_state_peak_count = stats.runtime_state_count;
        pending_refresh_assets.clear();
    }

    void AssetInstanceBridgeSystem::Rebuild(float, uint32_t max_instances_per_frame)
    {
        if (pending_rebuild.empty())
            return;

        uint32_t processed = 0;
        for (auto& weak_comp : pending_rebuild)
        {
            if (processed >= max_instances_per_frame)
                break;

            auto comp = weak_comp.lock();
            if (!comp)
                continue;

            const InstanceId instance_id = comp->GetInstanceID();
            if (instance_id == 0)
                continue;

            const auto* def = registry ? registry->Get(comp->GetAssetWorldID()) : nullptr;
            if (!def)
                continue;

            RuntimeState& state = runtime_states[instance_id];
            if (state.instance_id == 0)
            {
                state.instance_id = instance_id;
                state.proxy_handle = next_proxy_handle++;
            }

            state.asset_world_id = comp->GetAssetWorldID();
            state.resolved_version = def->version;
            state.last_observed_change_mask = comp->GetChangeMask();
            state.last_update_frame_index = world ? world->GetFrameIndex() : 0;
            ++state.rebuild_generation;

            comp->ClearAllChanges();
            ++processed;
        }

        stats.rebuild_count_frame = processed;
        stats.runtime_state_count = static_cast<uint32_t>(runtime_states.size());
        if (stats.runtime_state_count > stats.runtime_state_peak_count)
            stats.runtime_state_peak_count = stats.runtime_state_count;
    }

    void AssetInstanceBridgeSystem::SyncRender(float)
    {
        draw_packets.clear();
        draw_packet_bucket_counts.clear();
        draw_packet_secondary_bucket_counts.clear();
        draw_packets.reserve(runtime_states.size());

        std::unordered_map<InstanceId, std::shared_ptr<AssetInstanceComponent>> instance_components;
        if (world)
        {
            std::vector<std::shared_ptr<AssetInstanceComponent>> components;
            world->GetComponents(components);
            instance_components.reserve(components.size());

            for (const auto& comp : components)
            {
                if (!comp)
                    continue;

                const InstanceId instance_id = comp->GetInstanceID();
                if (instance_id == 0)
                    continue;

                instance_components[instance_id] = comp;
            }
        }

        for (const auto& pair : runtime_states)
        {
            const RuntimeState& state = pair.second;
            if (state.instance_id == 0)
                continue;

            uint16_t render_pass_id = 0;
            uint64_t material_bucket_key = state.asset_world_id;

            auto comp_it = instance_components.find(state.instance_id);
            if (comp_it != instance_components.end() && comp_it->second)
            {
                const auto& comp = comp_it->second;
                render_pass_id = static_cast<uint16_t>(comp->GetFlags() & 0xFFu);
                material_bucket_key = comp->GetOverrideRef().payload_ref != 0
                                      ? comp->GetOverrideRef().payload_ref
                                      : state.asset_world_id;
            }

            DrawPacket packet{};
            packet.instance_id = state.instance_id;
            packet.asset_world_id = state.asset_world_id;
            packet.resolved_version = state.resolved_version;
            packet.proxy_handle = state.proxy_handle;
            packet.render_pass_id = render_pass_id;
            packet.material_bucket_key = material_bucket_key;
            draw_packets.push_back(packet);

            ++draw_packet_bucket_counts[state.asset_world_id];
            ++draw_packet_secondary_bucket_counts[ComposeSecondaryBucketKey(render_pass_id, material_bucket_key)];
        }

        // Keep submission order deterministic to avoid frame-to-frame packet churn.
        std::sort(draw_packets.begin(), draw_packets.end(), [](const DrawPacket &lhs, const DrawPacket &rhs)
        {
            if (lhs.render_pass_id != rhs.render_pass_id)
                return lhs.render_pass_id < rhs.render_pass_id;
            if (lhs.material_bucket_key != rhs.material_bucket_key)
                return lhs.material_bucket_key < rhs.material_bucket_key;
            if (lhs.asset_world_id != rhs.asset_world_id)
                return lhs.asset_world_id < rhs.asset_world_id;
            return lhs.instance_id < rhs.instance_id;
        });

        // Hash the sorted packet stream to track cross-frame ordering stability.
        uint64_t order_hash = 1469598103934665603ull;
        for (const auto &packet : draw_packets)
        {
            HashCombine(order_hash, packet.instance_id);
            HashCombine(order_hash, packet.asset_world_id);
            HashCombine(order_hash, packet.render_pass_id);
            HashCombine(order_hash, packet.material_bucket_key);
        }

        if (has_last_draw_order_hash)
        {
            if (last_draw_order_hash == order_hash)
            {
                stats.order_matches_previous_frame = 1;
                ++stats.stable_order_match_count_total;
            }
            else
            {
                ++stats.order_changed_count_total;
            }
        }

        last_draw_order_hash = order_hash;
        has_last_draw_order_hash = true;

        stats.emitted_draw_packet_count_frame = static_cast<uint32_t>(draw_packets.size());
        stats.sorted_draw_packet_count_frame = static_cast<uint32_t>(draw_packets.size());
        stats.emitted_bucket_count_frame = static_cast<uint32_t>(draw_packet_bucket_counts.size());
        stats.emitted_secondary_bucket_count_frame = static_cast<uint32_t>(draw_packet_secondary_bucket_counts.size());
    }
}
