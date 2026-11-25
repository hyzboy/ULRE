#pragma once

#include<cstdint>

namespace hgl::ecs
{
    /**
     * 收集快路径（P3）的判据状态与统计（收集系统私有）。
     *
     * 分两块，都是"只由收集系统内部推进"的记账：
     * - **稳态判据 + 命中统计**：`steady` 表示上一帧**主帧 pass** 的收集是否为稳态
     *   （预扫描判定无任何材质工作需要做）。只有稳态才允许快路径接管，否则说明有实例
     *   正在等流式资源或在重试失败，必须让完整路径继续逐帧重试。
     * - **P1 干跑快照代数**：回答"假如 P2 已落地，本帧能不能复用上一帧的收集结果"。
     *   若沿用真实状态（无快照），报表会 100% 落在 `NoSnapshot`，等于什么都没测。
     */
    class CollectCacheState
    {
    private:
        bool last_main_collect_steady = false;

        uint64_t hits = 0;
        uint64_t fallbacks = 0;
        uint64_t last_items = 0;

        uint64_t dryrun_topology_rev = 0;
        bool dryrun_valid = false;

    public:
        bool IsLastMainCollectSteady() const { return last_main_collect_steady; }
        void SetLastMainCollectSteady(bool steady) { last_main_collect_steady = steady; }

        /// 快路径完整接管一次：记录成员数并累加命中
        void RecordHit(uint64_t items)
        {
            ++hits;
            last_items = items;
        }

        /// 尝试后被判据拒绝 / 中途发现不一致而回退
        void RecordFallback() { ++fallbacks; }

        uint64_t Hits() const { return hits; }
        uint64_t Fallbacks() const { return fallbacks; }
        uint64_t LastItems() const { return last_items; }

        /// 干跑代数：首次把当前代数当作"刚发布过快照"
        void EnsureDryRunInitialized(uint64_t topology_now)
        {
            if (dryrun_valid)
                return;

            dryrun_topology_rev = topology_now;
            dryrun_valid = true;
        }

        uint64_t DryRunTopologyRevision() const { return dryrun_topology_rev; }

        /// 判定为"拓扑脏"时刷新：真实实现会重建并发布一份新快照
        void AdvanceDryRun(uint64_t topology_now) { dryrun_topology_rev = topology_now; }
    };
}