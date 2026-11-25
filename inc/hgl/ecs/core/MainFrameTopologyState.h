#pragma once

#include<hgl/ecs/support/storage/PrimitiveTopologyLedger.h>
#include<hgl/ecs/support/storage/MainFrameTopologySnapshot.h>

namespace hgl::ecs
{
    /**
     * 主帧拓扑记账（世界私有）。
     *
     * 三件东西合为一体，因为它们共享同一套开关纪律：
     * - `PrimitiveTopologyLedger`：拓扑/动态事实的单调代数台帐（P1）。
     * - `MainFrameTopologySnapshot`：主帧拓扑快照（P2），只观测不驱动。
     * - 两个独立开关：快照本身（P2）与「让快照接管收集」的快路径（P3）。
     *   快路径**要求**快照开启，反之不成立 ⇒ 两个开关必须能独立打开。
     *
     * 关闭快照必须**完全不构建**（P2 零开销保证）；开关切换后的第一份快照没有
     * 可信基线 ⇒ 丢掉旧的比较基线与内容。
     */
    class MainFrameTopologyState
    {
    private:
        /// mutable：`MarkTopologyChanged` 是 const（"声明拓扑变更"不改世界可见状态），
        /// 而台帐是纯观测计数器 ⇒ 与旧成员保持同样的 mutable 契约。
        mutable PrimitiveTopologyLedger ledger;
        MainFrameTopologySnapshot snapshot;

        bool snapshot_enabled = false;
        bool collect_cache_enabled = false;

    public:
        PrimitiveTopologyLedger &Ledger() { return ledger; }
        const PrimitiveTopologyLedger &Ledger() const { return ledger; }

        MainFrameTopologySnapshot &Snapshot() { return snapshot; }
        const MainFrameTopologySnapshot &Snapshot() const { return snapshot; }

        void SetSnapshotEnabled(bool enabled)
        {
            if (snapshot_enabled == enabled)
                return;

            snapshot_enabled = enabled;

            // 开关切换后的第一份快照没有可信基线 ⇒ 丢掉旧的比较基线与内容。
            snapshot.Reset();
        }

        bool IsSnapshotEnabled() const { return snapshot_enabled; }

        void SetCollectCacheEnabled(bool enabled) { collect_cache_enabled = enabled; }

        bool IsCollectCacheEnabled() const { return collect_cache_enabled && snapshot_enabled; }

        void MarkTopology(TopologyChange kind, uint32_t count = 1) const { ledger.MarkTopology(kind, count); }
        void MarkDynamicFact(DynamicFactChange kind, uint32_t count = 1) const { ledger.MarkDynamicFact(kind, count); }

        void InvalidateSnapshot() { snapshot.Invalidate(); }

        /// 世界帧边界：推进台帐帧序号并轮询设备级行池代数
        void BeginFrame(graph::GlobalSSBOBufferRegistry *registry)
        {
            ledger.BeginFrame();

            if (registry)
                ledger.PollDeviceRowPools(registry);
        }
    };
}