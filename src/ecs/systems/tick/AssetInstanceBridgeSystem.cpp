#include <hgl/ecs/systems/tick/AssetInstanceBridgeSystem.h>

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/AssetInstanceComponent.h>

namespace hgl::ecs
{
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

    void AssetInstanceBridgeSystem::Collect(float)
    {
        stats.instance_count = 0;
        stats.unresolved_count = 0;
        stats.dirty_count = 0;
        stats.rebuild_count_frame = 0;
        pending_rebuild.clear();

        if (!world)
            return;

        std::vector<std::shared_ptr<AssetInstanceComponent>> components;
        world->GetComponents(components);

        stats.instance_count = static_cast<uint32_t>(components.size());

        for (const auto& comp : components)
        {
            if (!comp)
                continue;

            const bool resolved = registry && registry->Exists(comp->GetAssetWorldID());
            if (!resolved)
                ++stats.unresolved_count;

            const bool dirty = comp->GetChangeMask() != 0;
            if (dirty)
                ++stats.dirty_count;

            if (dirty && resolved)
                pending_rebuild.push_back(comp);
        }
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

            // Placeholder for runtime proxy build/patch in later G2/G3 steps.
            comp->ClearAllChanges();
            ++processed;
        }

        stats.rebuild_count_frame = processed;
    }

    void AssetInstanceBridgeSystem::SyncRender(float)
    {
        // Placeholder: render-domain synchronization is introduced in later phases.
    }
}
