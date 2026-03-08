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

    bool RunStress10kSmoke()
    {
        AssetWorldRegistry registry;
        AssetWorldDef def{};
        def.id = 4001ull;
        def.version = 1u;
        def.name = "StressAsset";
        if (!CheckOrLog(registry.Register(def), "register def"))
            return false;

        ECSContext ctx("BridgeStress10kSmoke");

        constexpr uint32_t kInstanceCount = 10000u;
        constexpr uint32_t kBudgetPerFrame = 512u;

        // Insert instances in reverse order; final packet order should still be deterministic.
        for (uint32_t i = 0; i < kInstanceCount; ++i)
        {
            Entity* e = ctx.CreateEntity<Entity>("StressInstance" + std::to_string(i));
            if (!CheckOrLog(e != nullptr, "create entity"))
                return false;

            auto c = e->AddComponent<AssetInstanceComponent>();
            if (!CheckOrLog(c != nullptr, "add asset instance component"))
                return false;

            c->SetAssetWorldID(4001ull);
            c->SetInstanceID(static_cast<InstanceId>(kInstanceCount - i));
            c->SetFlags(1u);
        }

        AssetInstanceBridgeSystem bridge;
        bridge.SetWorld(&ctx);
        bridge.SetRegistry(&registry);
        bridge.SetRebuildBudget(kBudgetPerFrame);
        bridge.Initialize();

        const uint32_t kFrameCountToConverge = (kInstanceCount + kBudgetPerFrame - 1u) / kBudgetPerFrame;
        for (uint32_t frame = 0; frame < kFrameCountToConverge; ++frame)
        {
            bridge.Update(0.016f);
            const auto s = bridge.GetStats();

            const uint32_t already_done = frame * kBudgetPerFrame;
            const uint32_t expected_rebuild = (kInstanceCount - already_done) > kBudgetPerFrame
                                              ? kBudgetPerFrame
                                              : (kInstanceCount - already_done);
            const uint32_t expected_runtime = already_done + expected_rebuild;

            if (!CheckOrLog(s.instance_count == kInstanceCount, "frame instance_count"))
                return false;
            if (!CheckOrLog(s.rebuild_count_frame == expected_rebuild, "frame rebuild_count_frame"))
                return false;
            if (!CheckOrLog(s.runtime_state_count == expected_runtime, "frame runtime_state_count"))
                return false;
            if (!CheckOrLog(s.emitted_draw_packet_count_frame == expected_runtime, "frame emitted_draw_packet_count_frame"))
                return false;
            if (!CheckOrLog(s.sorted_draw_packet_count_frame == expected_runtime, "frame sorted_draw_packet_count_frame"))
                return false;
            if (!CheckOrLog(s.rebuild_count_frame <= kBudgetPerFrame, "frame rebuild budget cap"))
                return false;
        }

        bridge.Update(0.016f);
        const auto stable = bridge.GetStats();

        if (!CheckOrLog(stable.instance_count == kInstanceCount, "stable instance_count"))
            return false;
        if (!CheckOrLog(stable.dirty_count == 0u, "stable dirty_count"))
            return false;
        if (!CheckOrLog(stable.rebuild_count_frame == 0u, "stable rebuild_count_frame"))
            return false;
        if (!CheckOrLog(stable.runtime_state_count == kInstanceCount, "stable runtime_state_count"))
            return false;
        if (!CheckOrLog(stable.runtime_state_peak_count == kInstanceCount, "stable runtime_state_peak_count"))
            return false;
        if (!CheckOrLog(stable.emitted_draw_packet_count_frame == kInstanceCount, "stable emitted_draw_packet_count_frame"))
            return false;
        if (!CheckOrLog(stable.sorted_draw_packet_count_frame == kInstanceCount, "stable sorted_draw_packet_count_frame"))
            return false;
        if (!CheckOrLog(stable.order_matches_previous_frame == 1u, "stable order_matches_previous_frame"))
            return false;
        if (!CheckOrLog(stable.stable_order_match_count_total >= 1u, "stable stable_order_match_count_total"))
            return false;

        const auto& packets = bridge.GetDrawPackets();
        if (!CheckOrLog(packets.size() == kInstanceCount, "stable packet size"))
            return false;
        if (!CheckOrLog(packets.front().instance_id == 1u, "stable first packet instance_id"))
            return false;
        if (!CheckOrLog(packets.back().instance_id == kInstanceCount, "stable last packet instance_id"))
            return false;

        if (!CheckOrLog(bridge.FindRuntimeState(1u) != nullptr, "state 1 exists"))
            return false;
        if (!CheckOrLog(bridge.FindRuntimeState(5000u) != nullptr, "state 5000 exists"))
            return false;
        if (!CheckOrLog(bridge.FindRuntimeState(10000u) != nullptr, "state 10000 exists"))
            return false;

        return true;
    }
}

int main()
{
    try
    {
        if (!RunStress10kSmoke())
        {
            std::cerr << "AssetInstanceBridgeStress10kSmoke: FAIL" << std::endl;
            return 1;
        }

        std::cout << "AssetInstanceBridgeStress10kSmoke: PASS" << std::endl;
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
