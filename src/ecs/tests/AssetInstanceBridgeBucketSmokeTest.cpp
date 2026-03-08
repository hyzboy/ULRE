#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/core/AssetWorldRegistry.h>
#include <hgl/ecs/components/AssetInstanceComponent.h>
#include <hgl/ecs/systems/tick/AssetInstanceBridgeSystem.h>

#include <exception>
#include <iostream>
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

        for (uint32_t i = 0; i < 3; ++i)
        {
            Entity* e = ctx.CreateEntity<Entity>("House_" + std::to_string(i));
            if (!e)
                return false;

            auto c = e->AddComponent<AssetInstanceComponent>();
            if (!c)
                return false;

            c->SetAssetWorldID(3001ull);
            c->SetInstanceID(static_cast<InstanceId>(i + 1));

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
            }
        }

        for (uint32_t i = 0; i < 2; ++i)
        {
            Entity* e = ctx.CreateEntity<Entity>("Tree_" + std::to_string(i));
            if (!e)
                return false;

            auto c = e->AddComponent<AssetInstanceComponent>();
            if (!c)
                return false;

            c->SetAssetWorldID(3002ull);
            c->SetInstanceID(static_cast<InstanceId>(100 + i));
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

        std::vector<InstanceId> ordered_instances_frame2;
        ordered_instances_frame2.reserve(bridge.GetDrawPackets().size());
        for (const auto &packet : bridge.GetDrawPackets())
            ordered_instances_frame2.push_back(packet.instance_id);

        if (ordered_instances_frame2 != ordered_instances)
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
