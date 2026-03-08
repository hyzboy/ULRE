#pragma once

#include <hgl/ecs/core/System.h>
#include <hgl/ecs/core/AssetWorldRegistry.h>

#include <vector>
#include <memory>
#include <cstdint>

namespace hgl::ecs
{
    class ECSContext;
    class AssetInstanceComponent;

    class AssetInstanceBridgeSystem : public System
    {
    public:
        struct Stats
        {
            uint32_t instance_count = 0;
            uint32_t unresolved_count = 0;
            uint32_t dirty_count = 0;
            uint32_t rebuild_count_frame = 0;
        };

    private:
        ECSContext* world = nullptr;
        IAssetWorldRegistry* registry = nullptr;
        uint32_t rebuild_budget = 256;
        Stats stats{};
        std::vector<std::weak_ptr<AssetInstanceComponent>> pending_rebuild;

    public:
        explicit AssetInstanceBridgeSystem(const std::string& name = "AssetInstanceBridgeSystem");

        void SetWorld(ECSContext* w) { world = w; }
        void SetRegistry(IAssetWorldRegistry* value) { registry = value; }
        IAssetWorldRegistry* GetRegistry() const { return registry; }

        void SetRebuildBudget(uint32_t value) { rebuild_budget = value == 0 ? 1 : value; }
        uint32_t GetRebuildBudget() const { return rebuild_budget; }

        const Stats& GetStats() const { return stats; }

        void Initialize() override;
        void Update(float deltaTime) override;

        void Collect(float deltaTime);
        void Rebuild(float deltaTime, uint32_t max_instances_per_frame);
        void SyncRender(float deltaTime);
    };
}
