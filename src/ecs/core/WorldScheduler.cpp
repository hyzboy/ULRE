#include<hgl/ecs/core/WorldScheduler.h>
#include<hgl/ecs/core/World.h>
#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/log/Log.h>
#include<cstdint>

namespace hgl::ecs
{
    namespace
    {
        uint64_t HashSubWorldPolicySample(uint64_t seed, const SubWorldComponent* sub_world)
        {
            if (!sub_world)
                return seed;

            const uint64_t ptr_bits = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(sub_world));
            const uint64_t logic_bit = sub_world->IsLogicIsolated() ? 0x9E3779B97F4A7C15ull : 0ull;
            const uint64_t render_bit = sub_world->IsRenderShared() ? 0xD6E8FEB86659FD93ull : 0ull;
            const uint64_t mix = ptr_bits ^ logic_bit ^ (render_bit >> 1);

            // Simple xorshift-like combine, stable within process lifetime.
            seed ^= mix + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2);
            return seed;
        }

        uint64_t ComputeSubWorldPolicySignature(const std::vector<std::shared_ptr<SubWorldComponent>>& sub_worlds)
        {
            uint64_t signature = 1469598103934665603ull;

            for (const auto& sub_world : sub_worlds)
            {
                signature = HashSubWorldPolicySample(signature, sub_world.get());
            }

            signature ^= static_cast<uint64_t>(sub_worlds.size()) * 1099511628211ull;
            return signature;
        }

        void SyncChildFrameIndex(ECSContext* parent_context, ECSContext* child_context)
        {
            if (!parent_context || !child_context)
                return;

            const uint32_t parent_frame = parent_context->GetFrameIndex();
            const uint32_t child_frame = child_context->GetFrameIndex();

            if (parent_frame != child_frame)
                child_context->SetFrameIndex(parent_frame);
        }
    }

    bool WorldScheduler::RefreshSubWorldListsIfNeeded(FlatWorldRecord& record)
    {
        if (!record.context)
        {
            record.subworld_component_count = 0;
            record.subworld_policy_signature = 0;
            record.lists_valid = true;
            record.logic_subworlds.clear();
            record.bridge_subworlds.clear();
            record.isolated_render_subworlds.clear();
            return true;
        }

        std::vector<std::shared_ptr<SubWorldComponent>> sub_worlds;
        record.context->GetComponents(sub_worlds);

        const size_t new_count = sub_worlds.size();
        const uint64_t new_signature = ComputeSubWorldPolicySignature(sub_worlds);

        if (record.lists_valid &&
            record.subworld_component_count == new_count &&
            record.subworld_policy_signature == new_signature)
        {
            return false;
        }

        record.logic_subworlds.clear();
        record.bridge_subworlds.clear();
        record.isolated_render_subworlds.clear();

        for (const auto& sub_world : sub_worlds)
        {
            if (!sub_world)
                continue;

            if (sub_world->IsLogicIsolated())
                record.logic_subworlds.push_back(sub_world.get());

            if (sub_world->IsLogicIsolated() && sub_world->IsRenderShared())
                record.bridge_subworlds.push_back(sub_world.get());

            if (!sub_world->IsRenderShared())
                record.isolated_render_subworlds.push_back(sub_world.get());
        }

        record.subworld_component_count = new_count;
        record.subworld_policy_signature = new_signature;
        record.lists_valid = true;
        return true;
    }

    void WorldScheduler::FlattenWorldTree(World* root, ECSContext* parent_context)
    {
        if (!root)
            return;

        FlatWorldRecord record;
        record.world = root;
        record.context = root->GetContext();
        record.parent_context = parent_context;
        record.lists_valid = false;
        flat_worlds.emplace_back(std::move(record));

        const auto& children = root->GetChildren();
        for (const auto& child : children)
        {
            if (!child)
                continue;

            FlattenWorldTree(child.get(), root->GetContext());
        }
    }

    void WorldScheduler::Rebuild(World* root)
    {
        if (!root)
        {
            flat_worlds.clear();
            flat_logic_subworlds.clear();
            stats = SchedulerStats{};
            cached_root = nullptr;
            topology_dirty = true;
            return;
        }

        const bool can_incremental_refresh = !topology_dirty && cached_root == root;

        if (!can_incremental_refresh)
        {
            flat_worlds.clear();
            cached_root = root;
            FlattenWorldTree(root, nullptr);
            topology_dirty = false;
        }

        flat_logic_subworlds.clear();
        stats = SchedulerStats{};
        size_t refreshed_record_count = 0;

        for (auto& record : flat_worlds)
        {
            if (RefreshSubWorldListsIfNeeded(record))
                ++refreshed_record_count;

            for (auto* sub_world : record.logic_subworlds)
                flat_logic_subworlds.push_back(sub_world);
        }

        stats.flat_world_count = flat_worlds.size();
        stats.logic_subworld_count = flat_logic_subworlds.size();

        for (const auto& record : flat_worlds)
        {
            stats.bridge_subworld_count += record.bridge_subworlds.size();
            stats.isolated_render_subworld_count += record.isolated_render_subworlds.size();
        }

    #if ULRE_ECS_DEBUG_API
        static bool logged_once = false;
        if (!logged_once)
        {
            logged_once = true;
            GLogDebug("[WorldScheduler] Rebuild: mode=%s worlds=%zu logic=%zu bridge=%zu isolated_render=%zu",
                      can_incremental_refresh ? "incremental" : "full",
                      stats.flat_world_count,
                      stats.logic_subworld_count,
                      stats.bridge_subworld_count,
                      stats.isolated_render_subworld_count);
        }

        static bool logged_refresh_once = false;
        if (!logged_refresh_once)
        {
            logged_refresh_once = true;
            GLogDebug("[WorldScheduler] Rebuild refresh records=%zu/%zu",
                      refreshed_record_count,
                      flat_worlds.size());
        }
    #endif
    }

    void WorldScheduler::Tick(float delta_time)
    {
        for (auto& record : flat_worlds)
        {
            if (!record.world || !record.world->IsActive())
                continue;

            if (record.context)
            {
                SyncChildFrameIndex(record.parent_context, record.context);
                record.context->SetSubWorldAutoUpdate(false);
                record.context->Tick(delta_time);
            }

            for (auto* sub_world : record.logic_subworlds)
            {
                if (sub_world)
                    sub_world->UpdateSubWorld(delta_time);
            }
        }
    }

    void WorldScheduler::Render(graph::RenderCmdBuffer* cmd, float delta_time)
    {
        for (auto& record : flat_worlds)
        {
            if (!record.world || !record.world->IsActive())
                continue;

            if (record.context)
            {
                SyncChildFrameIndex(record.parent_context, record.context);

                for (auto* sub_world : record.bridge_subworlds)
                {
                    if (sub_world)
                        sub_world->SyncSharedRenderBridge(delta_time);
                }

                record.context->SetSubWorldAutoUpdate(false);
                record.context->Render(cmd, delta_time);
            }

            for (auto* sub_world : record.isolated_render_subworlds)
            {
                if (sub_world)
                    sub_world->RenderSubWorld(cmd, delta_time);
            }
        }
    }
}
