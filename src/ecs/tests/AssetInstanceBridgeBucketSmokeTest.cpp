#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/core/AssetWorldRegistry.h>
#include <hgl/ecs/components/AssetInstanceComponent.h>
#include <hgl/ecs/systems/tick/AssetInstanceBridgeSystem.h>

#include <exception>
#include <iostream>

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

        if (bridge.GetDrawPackets().size() != 5u)
            return false;

        if (bridge.GetDrawPacketCountForAsset(3001ull) != 3u)
            return false;
        if (bridge.GetDrawPacketCountForAsset(3002ull) != 2u)
            return false;
        if (bridge.GetDrawPacketCountForAsset(9999ull) != 0u)
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
