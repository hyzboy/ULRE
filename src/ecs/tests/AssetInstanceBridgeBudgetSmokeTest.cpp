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
    bool CheckOrLog(bool condition, const char* message)
    {
        if (!condition)
            std::cerr << "CHECK FAILED: " << message << std::endl;

        return condition;
    }

    bool RunBudgetSmoke()
    {
        AssetWorldRegistry registry;
        AssetWorldDef def{};
        def.id = 2001ull;
        def.version = 5u;
        def.name = "BudgetAsset";
        if (!CheckOrLog(registry.Register(def), "register def"))
            return false;

        ECSContext ctx("BridgeBudgetSmoke");

        constexpr uint32_t kInstanceCount = 5;
        for (uint32_t i = 0; i < kInstanceCount; ++i)
        {
            Entity* e = ctx.CreateEntity<Entity>("Instance" + std::to_string(i));
            if (!CheckOrLog(e != nullptr, "create entity"))
                return false;

            auto c = e->AddComponent<AssetInstanceComponent>();
            if (!CheckOrLog(c != nullptr, "add asset instance component"))
                return false;

            c->SetAssetWorldID(2001ull);
            c->SetInstanceID(static_cast<InstanceId>(i + 1));
        }

        // Add one invalid-id instance; bridge should count and skip it.
        Entity* invalid_entity = ctx.CreateEntity<Entity>("InvalidInstance");
        if (!CheckOrLog(invalid_entity != nullptr, "create invalid entity"))
            return false;

        auto invalid_comp = invalid_entity->AddComponent<AssetInstanceComponent>();
        if (!CheckOrLog(invalid_comp != nullptr, "add invalid asset instance component"))
            return false;

        invalid_comp->SetAssetWorldID(2001ull);
        invalid_comp->SetInstanceID(0ull);

        AssetInstanceBridgeSystem bridge;
        bridge.SetWorld(&ctx);
        bridge.SetRegistry(&registry);
        bridge.SetRebuildBudget(2);
        bridge.Initialize();

        bridge.Update(0.016f);
        auto s1 = bridge.GetStats();
        if (!CheckOrLog(s1.instance_count == kInstanceCount + 1u, "s1 instance_count"))
            return false;
        if (!CheckOrLog(s1.invalid_instance_id_count_frame == 1u, "s1 invalid_instance_id_count_frame"))
            return false;
        if (!CheckOrLog(s1.rebuild_count_frame == 2u, "s1 rebuild_count_frame"))
            return false;
        if (!CheckOrLog(s1.runtime_state_count == 2u, "s1 runtime_state_count"))
            return false;
        if (!CheckOrLog(s1.emitted_draw_packet_count_frame == 2u, "s1 emitted_draw_packet_count_frame"))
            return false;

        bridge.Update(0.016f);
        auto s2 = bridge.GetStats();
        if (!CheckOrLog(s2.instance_count == kInstanceCount + 1u, "s2 instance_count"))
            return false;
        if (!CheckOrLog(s2.invalid_instance_id_count_frame == 1u, "s2 invalid_instance_id_count_frame"))
            return false;
        if (!CheckOrLog(s2.rebuild_count_frame == 2u, "s2 rebuild_count_frame"))
            return false;
        if (!CheckOrLog(s2.runtime_state_count == 4u, "s2 runtime_state_count"))
            return false;
        if (!CheckOrLog(s2.emitted_draw_packet_count_frame == 4u, "s2 emitted_draw_packet_count_frame"))
            return false;

        bridge.Update(0.016f);
        auto s3 = bridge.GetStats();
        if (!CheckOrLog(s3.instance_count == kInstanceCount + 1u, "s3 instance_count"))
            return false;
        if (!CheckOrLog(s3.invalid_instance_id_count_frame == 1u, "s3 invalid_instance_id_count_frame"))
            return false;
        if (!CheckOrLog(s3.rebuild_count_frame == 1u, "s3 rebuild_count_frame"))
            return false;
        if (!CheckOrLog(s3.runtime_state_count == 5u, "s3 runtime_state_count"))
            return false;
        if (!CheckOrLog(s3.runtime_state_peak_count == 5u, "s3 runtime_state_peak_count"))
            return false;
        if (!CheckOrLog(s3.emitted_draw_packet_count_frame == 5u, "s3 emitted_draw_packet_count_frame"))
            return false;

        bridge.Update(0.016f);
        auto s4 = bridge.GetStats();
        if (!CheckOrLog(s4.instance_count == kInstanceCount + 1u, "s4 instance_count"))
            return false;
        if (!CheckOrLog(s4.invalid_instance_id_count_frame == 1u, "s4 invalid_instance_id_count_frame"))
            return false;
        if (!CheckOrLog(s4.dirty_count == 0u, "s4 dirty_count"))
            return false;
        if (!CheckOrLog(s4.rebuild_count_frame == 0u, "s4 rebuild_count_frame"))
            return false;
        if (!CheckOrLog(s4.runtime_state_count == 5u, "s4 runtime_state_count"))
            return false;
        if (!CheckOrLog(s4.runtime_state_peak_count == 5u, "s4 runtime_state_peak_count"))
            return false;
        if (!CheckOrLog(s4.emitted_draw_packet_count_frame == 5u, "s4 emitted_draw_packet_count_frame"))
            return false;

        const auto* state3 = bridge.FindRuntimeState(3ull);
        if (!CheckOrLog(state3 != nullptr, "state3 exists"))
            return false;
        if (!CheckOrLog(state3->asset_world_id == 2001ull, "state3 asset_world_id"))
            return false;
        if (!CheckOrLog(state3->resolved_version == 5u, "state3 resolved_version"))
            return false;
        if (!CheckOrLog(state3->last_observed_change_mask != 0u, "state3 last_observed_change_mask"))
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
