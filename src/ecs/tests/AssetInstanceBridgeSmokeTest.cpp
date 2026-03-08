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
    bool RunSmoke()
    {
        AssetWorldRegistry registry;
        AssetWorldDef def{};
        def.id = 1001ull;
        def.version = 1u;
        def.name = "HouseAsset";
        if (!registry.Register(def))
            return false;

        ECSContext ctx("BridgeSmoke");

        Entity* resolved_entity = ctx.CreateEntity<Entity>("Resolved");
        if (!resolved_entity)
            return false;

        auto resolved_instance = resolved_entity->AddComponent<AssetInstanceComponent>();
        if (!resolved_instance)
            return false;

        resolved_instance->SetAssetWorldID(1001ull);
        resolved_instance->SetInstanceID(1ull);

        Entity* unresolved_entity = ctx.CreateEntity<Entity>("Unresolved");
        if (!unresolved_entity)
            return false;

        auto unresolved_instance = unresolved_entity->AddComponent<AssetInstanceComponent>();
        if (!unresolved_instance)
            return false;

        unresolved_instance->SetAssetWorldID(9999ull);
        unresolved_instance->SetInstanceID(2ull);

        AssetInstanceBridgeSystem bridge;
        bridge.SetWorld(&ctx);
        bridge.SetRegistry(&registry);
        bridge.SetRebuildBudget(8);
        bridge.Initialize();
        bridge.Update(0.016f);

        const auto& stats = bridge.GetStats();

        if (stats.instance_count != 2u)
            return false;
        if (stats.unresolved_count != 1u)
            return false;
        if (stats.dirty_count != 2u)
            return false;
        if (stats.rebuild_count_frame != 1u)
            return false;

        return true;
    }
}

int main()
{
    try
    {
        if (!RunSmoke())
        {
            std::cerr << "AssetInstanceBridgeSmoke: FAIL" << std::endl;
            return 1;
        }

        std::cout << "AssetInstanceBridgeSmoke: PASS" << std::endl;
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
