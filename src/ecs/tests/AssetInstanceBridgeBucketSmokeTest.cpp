#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/core/AssetWorldRegistry.h>
#include <hgl/ecs/components/AssetInstanceComponent.h>
#include <hgl/ecs/systems/tick/AssetInstanceBridgeSystem.h>

#include <exception>
#include <iostream>
#include <memory>
#include <vector>

using namespace hgl::ecs;

namespace
{
    bool RunBucketSmoke()
    {
        AssetWorldRegistry registry;

        AssetWorldDef house{};
        house.id = 3001ull;
        house.version = 2u;
        house.name = "House";
        if (!registry.Register(house))
            return false;

        AssetWorldDef tree{};
        tree.id = 3002ull;
        tree.version = 4u;
        tree.name = "Tree";
        if (!registry.Register(tree))
            return false;

        ECSContext ctx("BridgeBucketSmoke");
        std::shared_ptr<AssetInstanceComponent> jitter_target;

        const InstanceId house_instance_ids[3] = {2u, 1u, 3u};
        for (uint32_t i = 0; i < 3; ++i)
        {
            Entity* e = ctx.CreateEntity<Entity>("House_" + std::to_string(i));
            if (!e)
                return false;

            auto c = e->AddComponent<AssetInstanceComponent>();
            if (!c)
                return false;

            c->SetAssetWorldID(3001ull);
            c->SetInstanceID(house_instance_ids[i]);

            // Two pass/material groups inside the same asset bucket.
            if (i < 2)
            {
                c->SetFlags(1u);
                AssetOverrideRef ref{};
                ref.payload_ref = 7001ull;
                ref.revision = 1u;
                c->SetOverrideRef(ref);
            }
            else
            {
                c->SetFlags(2u);
                AssetOverrideRef ref{};
                ref.payload_ref = 7002ull;
                ref.revision = 1u;
                c->SetOverrideRef(ref);

                // Keep one component handle for later bucket jitter simulation.
                if (house_instance_ids[i] == 3u)
                    jitter_target = c;
            }
        }

        const InstanceId tree_instance_ids[2] = {101u, 100u};
        for (uint32_t i = 0; i < 2; ++i)
        {
            Entity* e = ctx.CreateEntity<Entity>("Tree_" + std::to_string(i));
            if (!e)
                return false;

            auto c = e->AddComponent<AssetInstanceComponent>();
            if (!c)
                return false;

            c->SetAssetWorldID(3002ull);
            c->SetInstanceID(tree_instance_ids[i]);
            c->SetFlags(1u);
            AssetOverrideRef ref{};
            ref.payload_ref = 9001ull;
            ref.revision = 1u;
            c->SetOverrideRef(ref);
        }

        AssetInstanceBridgeSystem bridge;
        bridge.SetWorld(&ctx);
        bridge.SetRegistry(&registry);
        bridge.SetRebuildBudget(32);
        bridge.Initialize();

        bridge.Update(0.016f);
        const auto& stats = bridge.GetStats();

        if (stats.instance_count != 5u)
            return false;
        if (stats.rebuild_count_frame != 5u)
            return false;
        if (stats.runtime_state_count != 5u)
            return false;
        if (stats.emitted_draw_packet_count_frame != 5u)
            return false;
        if (stats.emitted_bucket_count_frame != 2u)
            return false;
        if (stats.emitted_secondary_bucket_count_frame != 3u)
            return false;

        if (bridge.GetDrawPackets().size() != 5u)
            return false;

        std::vector<InstanceId> ordered_instances;
        ordered_instances.reserve(bridge.GetDrawPackets().size());
        for (const auto &packet : bridge.GetDrawPackets())
            ordered_instances.push_back(packet.instance_id);

        const std::vector<InstanceId> expected_order = {1u, 2u, 100u, 101u, 3u};
        if (ordered_instances != expected_order)
            return false;

        if (stats.sorted_draw_packet_count_frame != 5u)
            return false;
        if (stats.order_matches_previous_frame != 0u)
            return false;
        if (stats.stable_order_match_count_total != 0u)
            return false;
        if (stats.order_changed_count_total != 0u)
            return false;

        if (bridge.GetDrawPacketCountForAsset(3001ull) != 3u)
            return false;
        if (bridge.GetDrawPacketCountForAsset(3002ull) != 2u)
            return false;
        if (bridge.GetDrawPacketCountForAsset(9999ull) != 0u)
            return false;

        if (bridge.GetDrawPacketCountForSecondaryBucket(1u, 7001ull) != 2u)
            return false;
        if (bridge.GetDrawPacketCountForSecondaryBucket(2u, 7002ull) != 1u)
            return false;
        if (bridge.GetDrawPacketCountForSecondaryBucket(1u, 9001ull) != 2u)
            return false;
        if (bridge.GetDrawPacketCountForSecondaryBucket(3u, 7001ull) != 0u)
            return false;

        bridge.Update(0.016f);
        const auto &stats2 = bridge.GetStats();
        if (stats2.rebuild_count_frame != 0u)
            return false;
        if (stats2.emitted_draw_packet_count_frame != 5u)
            return false;
        if (stats2.sorted_draw_packet_count_frame != 5u)
            return false;
        if (stats2.order_matches_previous_frame != 1u)
            return false;
        if (stats2.stable_order_match_count_total != 1u)
            return false;
        if (stats2.order_changed_count_total != 0u)
            return false;

        std::vector<InstanceId> ordered_instances_frame2;
        ordered_instances_frame2.reserve(bridge.GetDrawPackets().size());
        for (const auto &packet : bridge.GetDrawPackets())
            ordered_instances_frame2.push_back(packet.instance_id);

        if (ordered_instances_frame2 != ordered_instances)
            return false;

        if (!jitter_target)
            return false;

        // Move only one instance into a different secondary bucket and verify order delta.
        jitter_target->SetFlags(1u);
        AssetOverrideRef jitter_ref{};
        jitter_ref.payload_ref = 7001ull;
        jitter_ref.revision = 2u;
        jitter_target->SetOverrideRef(jitter_ref);

        bridge.Update(0.016f);
        const auto &stats3 = bridge.GetStats();
        if (stats3.rebuild_count_frame != 1u)
            return false;
        if (stats3.order_matches_previous_frame != 0u)
            return false;
        if (stats3.stable_order_match_count_total != 1u)
            return false;
        if (stats3.order_changed_count_total != 1u)
            return false;

        std::vector<InstanceId> ordered_instances_frame3;
        ordered_instances_frame3.reserve(bridge.GetDrawPackets().size());
        for (const auto &packet : bridge.GetDrawPackets())
            ordered_instances_frame3.push_back(packet.instance_id);

        const std::vector<InstanceId> expected_after_jitter = {1u, 2u, 3u, 100u, 101u};
        if (ordered_instances_frame3 != expected_after_jitter)
            return false;

        bridge.Update(0.016f);
        const auto &stats4 = bridge.GetStats();
        if (stats4.rebuild_count_frame != 0u)
            return false;
        if (stats4.order_matches_previous_frame != 1u)
            return false;
        if (stats4.stable_order_match_count_total != 2u)
            return false;
        if (stats4.order_changed_count_total != 1u)
            return false;

        return true;
    }
}

int main()
{
    try
    {
        if (!RunBucketSmoke())
        {
            std::cerr << "AssetInstanceBridgeBucketSmoke: FAIL" << std::endl;
            return 1;
        }

        std::cout << "AssetInstanceBridgeBucketSmoke: PASS" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 2;
    }
    catch (...)
    {
        std::cerr << "Unknown exception" << std::endl;
        return 3;
    }
}
