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
        if (stats.runtime_state_count != 1u)
            return false;

        const auto* resolved_state = bridge.FindRuntimeState(1ull);
        if (!resolved_state)
            return false;
        if (resolved_state->asset_world_id != 1001ull)
            return false;
        if (resolved_state->resolved_version != 1u)
            return false;

        // Dirty-only path: resolved instance should now be clean; unresolved one remains dirty.
        bridge.Update(0.016f);
        const auto& stats_second = bridge.GetStats();
        if (stats_second.instance_count != 2u)
            return false;
        if (stats_second.unresolved_count != 1u)
            return false;
        if (stats_second.dirty_count != 1u)
            return false;
        if (stats_second.rebuild_count_frame != 0u)
            return false;
        if (stats_second.runtime_state_count != 1u)
            return false;

        // Unresolved becomes resolved later.
        AssetWorldDef late_def{};
        late_def.id = 9999ull;
        late_def.version = 2u;
        late_def.name = "LateResolved";
        if (!registry.Register(late_def))
            return false;

        bridge.Update(0.016f);
        const auto& stats_third = bridge.GetStats();
        if (stats_third.instance_count != 2u)
            return false;
        if (stats_third.unresolved_count != 0u)
            return false;
        if (stats_third.dirty_count != 1u)
            return false;
        if (stats_third.rebuild_count_frame != 1u)
            return false;
        if (stats_third.runtime_state_count != 2u)
            return false;

        const auto* late_state = bridge.FindRuntimeState(2ull);
        if (!late_state)
            return false;
        if (late_state->asset_world_id != 9999ull)
            return false;
        if (late_state->resolved_version != 2u)
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
