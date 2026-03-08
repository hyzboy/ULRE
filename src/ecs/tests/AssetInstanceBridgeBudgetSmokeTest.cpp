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
    bool RunBudgetSmoke()
    {
        AssetWorldRegistry registry;
        AssetWorldDef def{};
        def.id = 2001ull;
        def.version = 5u;
        def.name = "BudgetAsset";
        if (!registry.Register(def))
            return false;

        ECSContext ctx("BridgeBudgetSmoke");

        constexpr uint32_t kInstanceCount = 5;
        for (uint32_t i = 0; i < kInstanceCount; ++i)
        {
            Entity* e = ctx.CreateEntity<Entity>("Instance" + std::to_string(i));
            if (!e)
                return false;

            auto c = e->AddComponent<AssetInstanceComponent>();
            if (!c)
                return false;

            c->SetAssetWorldID(2001ull);
            c->SetInstanceID(static_cast<InstanceId>(i + 1));
        }

        AssetInstanceBridgeSystem bridge;
        bridge.SetWorld(&ctx);
        bridge.SetRegistry(&registry);
        bridge.SetRebuildBudget(2);
        bridge.Initialize();

        bridge.Update(0.016f);
        auto s1 = bridge.GetStats();
        if (s1.instance_count != kInstanceCount || s1.rebuild_count_frame != 2u || s1.runtime_state_count != 2u)
            return false;

        bridge.Update(0.016f);
        auto s2 = bridge.GetStats();
        if (s2.instance_count != kInstanceCount || s2.rebuild_count_frame != 2u || s2.runtime_state_count != 4u)
            return false;

        bridge.Update(0.016f);
        auto s3 = bridge.GetStats();
        if (s3.instance_count != kInstanceCount || s3.rebuild_count_frame != 1u || s3.runtime_state_count != 5u)
            return false;

        bridge.Update(0.016f);
        auto s4 = bridge.GetStats();
        if (s4.instance_count != kInstanceCount || s4.dirty_count != 0u || s4.rebuild_count_frame != 0u || s4.runtime_state_count != 5u)
            return false;

        const auto* state3 = bridge.FindRuntimeState(3ull);
        if (!state3)
            return false;
        if (state3->asset_world_id != 2001ull || state3->resolved_version != 5u)
            return false;
        if (state3->last_observed_change_mask == 0u)
            return false;

        return true;
    }
}

int main()
{
    try
    {
        if (!RunBudgetSmoke())
        {
            std::cerr << "AssetInstanceBridgeBudgetSmoke: FAIL" << std::endl;
            return 1;
        }

        std::cout << "AssetInstanceBridgeBudgetSmoke: PASS" << std::endl;
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
