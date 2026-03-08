#include <hgl/ecs/systems/tick/AssetInstanceBridgeSystem.h>

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/AssetInstanceComponent.h>

#include <unordered_set>

namespace hgl::ecs
{
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
        stats.emitted_bucket_count_frame = 0;
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
        draw_packets.reserve(runtime_states.size());

        for (const auto& pair : runtime_states)
        {
            const RuntimeState& state = pair.second;
            if (state.instance_id == 0)
                continue;

            DrawPacket packet{};
            packet.instance_id = state.instance_id;
            packet.asset_world_id = state.asset_world_id;
            packet.resolved_version = state.resolved_version;
            packet.proxy_handle = state.proxy_handle;
            draw_packets.push_back(packet);

            ++draw_packet_bucket_counts[state.asset_world_id];
        }

        stats.emitted_draw_packet_count_frame = static_cast<uint32_t>(draw_packets.size());
        stats.emitted_bucket_count_frame = static_cast<uint32_t>(draw_packet_bucket_counts.size());
    }
}
