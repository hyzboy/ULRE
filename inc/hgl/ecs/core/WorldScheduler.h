#pragma once

#include<vector>

namespace hgl
{
    namespace graph
    {
        class RenderCmdBuffer;
    }

    namespace ecs
    {
        class World;
        class SubWorldComponent;
        class ECSContext;

        class WorldScheduler final
        {
        public:
            struct SchedulerStats
            {
                size_t flat_world_count = 0;
                size_t logic_subworld_count = 0;
                size_t bridge_subworld_count = 0;
                size_t isolated_render_subworld_count = 0;
            };

        private:
            struct FlatWorldRecord
            {
                World* world = nullptr;
                ECSContext* context = nullptr;
                ECSContext* parent_context = nullptr;
                std::vector<SubWorldComponent*> logic_subworlds;
                std::vector<SubWorldComponent*> bridge_subworlds;
                std::vector<SubWorldComponent*> isolated_render_subworlds;
            };

            std::vector<FlatWorldRecord> flat_worlds;
            std::vector<SubWorldComponent*> flat_logic_subworlds;
            SchedulerStats stats;
            World* cached_root = nullptr;
            bool topology_dirty = true;

        private:
            void FlattenWorldTree(World* root, ECSContext* parent_context);
            void CollectSubWorldLists(FlatWorldRecord& record);

        public:
            void MarkTopologyDirty() { topology_dirty = true; }
            void Rebuild(World* root);
            void Tick(float delta_time);
            void Render(graph::RenderCmdBuffer* cmd, float delta_time);

            size_t GetFlatWorldCount() const { return flat_worlds.size(); }
            size_t GetFlatLogicSubWorldCount() const { return flat_logic_subworlds.size(); }
            const SchedulerStats& GetStats() const { return stats; }
        };
    }//namespace ecs
}//namespace hgl
