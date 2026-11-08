#include<hgl/ecs/support/storage/PrimitiveTopologyLedger.h>

#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    // 池代数表的宽度必须覆盖真实枚举项数，否则 `PollDeviceRowPools` 会静默漏采样。
    static_assert(static_cast<size_t>(graph::GlobalSSBOTypeCount) <= PrimitiveTopologyLedger::kPoolSlotCount,
                  "PrimitiveTopologyLedger::kPoolSlotCount 小于 GlobalSSBOTypeCount，"
                  "请同步放大 kPoolSlotCount，否则池代数轮询会漏采样");

    namespace
    {
        constexpr size_t TopoSlot(const TopologyChange kind)
        {
            return static_cast<size_t>(kind);
        }

        constexpr size_t FactSlot(const DynamicFactChange kind)
        {
            return static_cast<size_t>(kind);
        }

        constexpr size_t RejectSlot(const CacheRejectReason reason)
        {
            return static_cast<size_t>(reason);
        }

        constexpr size_t kTopoCount = static_cast<size_t>(TopologyChange::Count);
        constexpr size_t kFactCount = static_cast<size_t>(DynamicFactChange::Count);
        constexpr size_t kRejectCount = static_cast<size_t>(CacheRejectReason::Count);

        const char *kTopologyNames[kTopoCount] =
        {
            "EntityStructure",
            "GeometryBinding",
            "ModelTemplate",
            "MaterialBinding",
            "PassAndTarget",
        };

        const char *kFactNames[kFactCount] =
        {
            "Visibility",
            "PartMask",
            "CustomData",
            "PrevTransform",
            "MaterialContent",
        };

        const char *kRejectNames[kRejectCount] =
        {
            "Eligible",
            "NotMainSwapchainFrame",
            "GpuCullNotEngaged",
            "NoSnapshot",
            "SnapshotNotReady",
            "TopologyDirty",
        };

        /// 池类型 → 拓扑/事实类别的唯一映射。
        /// 新增 `GlobalSSBOType` 时这里必须给出归类，否则 `PollDeviceRowPools`
        /// 只会在 debug 日志里报一次「未归类」，而漏掉的池等于未封闭的失效源。
        bool MapPoolType(const graph::GlobalSSBOType type,
                         TopologyChange &out_topology,
                         DynamicFactChange &out_fact)
        {
            using T = graph::GlobalSSBOType;

            switch (type)
            {
                case T::MeshDrawParams:
                case T::GeometryAABB:
                    out_topology = TopologyChange::GeometryBinding;
                    out_fact     = DynamicFactChange::MaterialContent;
                    return true;

                case T::PBRSurface:
                case T::EmissiveSurface:
                case T::TransmissionSurface:
                case T::PBRAdvancedSurface:
                case T::MaterialData:
                    out_topology = TopologyChange::MaterialBinding;
                    out_fact     = DynamicFactChange::MaterialContent;
                    return true;

                case T::ModelTemplatePool:
                case T::ModelPartPool:
                    out_topology = TopologyChange::ModelTemplate;
                    out_fact     = DynamicFactChange::PartMask;
                    return true;

                case T::InstanceCustomDataPool:
                    out_topology = TopologyChange::MaterialBinding;
                    out_fact     = DynamicFactChange::CustomData;
                    return true;

                default:
                    break;
            }

            return false;
        }
    }//namespace

    void PrimitiveTopologyLedger::MarkTopology(const TopologyChange kind, const uint32_t count)
    {
        if (kind >= TopologyChange::Count || count == 0)
            return;

        topology_rev[TopoSlot(kind)] += count;
        topology_total              += count;
        topology_last_frame[TopoSlot(kind)] = frame_serial;
    }

    void PrimitiveTopologyLedger::MarkDynamicFact(const DynamicFactChange kind, const uint32_t count)
    {
        if (kind >= DynamicFactChange::Count || count == 0)
            return;

        fact_rev[FactSlot(kind)] += count;
        fact_total               += count;
    }

    bool PrimitiveTopologyLedger::PollDeviceRowPools(graph::GlobalSSBOBufferRegistry *registry)
    {
        if (!registry || !registry->IsInitialized())
            return false;

        bool changed = false;

        for (uint32_t i = 0; i < static_cast<uint32_t>(graph::GlobalSSBOTypeCount); ++i)
        {
            if (i >= kPoolSlotCount)
                break;

            const auto type = static_cast<graph::GlobalSSBOType>(i);

            TopologyChange    topology = TopologyChange::EntityStructure;
            DynamicFactChange fact     = DynamicFactChange::MaterialContent;

            if (!MapPoolType(type, topology, fact))
                continue;

            const graph::ActiveRowPool *pool = registry->GetPool(type);
            if (!pool)
                continue;

            const uint64_t layout  = pool->GetLayoutVersion();
            const uint64_t content = pool->GetContentVersion();

            // 首次采样只建立基线：把「世界启动前就已存在的行」当成既有状态，
            // 否则每一帧第一次轮询都会把整池算成「刚变更」，快照永远无法发布。
            if (polled_valid)
            {
                if (polled_layout[i] != layout)
                {
                    MarkTopology(topology);
                    ++stat_pool_layout_changes;
                    changed = true;
                }

                if (polled_content[i] != content)
                {
                    MarkDynamicFact(fact);
                    ++stat_pool_content_changes;
                    changed = true;
                }
            }

            polled_layout[i]  = layout;
            polled_content[i] = content;
        }

        polled_valid = true;
        return changed;
    }

    uint64_t PrimitiveTopologyLedger::GetTopologyRevision(const TopologyChange kind) const
    {
        return (kind < TopologyChange::Count) ? topology_rev[TopoSlot(kind)] : 0;
    }

    uint64_t PrimitiveTopologyLedger::GetDynamicFactRevision(const DynamicFactChange kind) const
    {
        return (kind < DynamicFactChange::Count) ? fact_rev[FactSlot(kind)] : 0;
    }

    uint32_t PrimitiveTopologyLedger::GetTopologyLastFrame(const TopologyChange kind) const
    {
        return (kind < TopologyChange::Count) ? topology_last_frame[TopoSlot(kind)] : 0;
    }

    CacheRejectReason PrimitiveTopologyLedger::Evaluate(const CollectCacheInputs &in) const
    {
        // 顺序即语义：先排除「根本不是候选」的硬条件，最后才比代数。
        if (!in.is_main_swapchain_frame)
            return CacheRejectReason::NotMainSwapchainFrame;

        if (!in.gpu_cull_engaged)
            return CacheRejectReason::GpuCullNotEngaged;

        if (!in.has_snapshot)
            return CacheRejectReason::NoSnapshot;

        if (!in.snapshot_ready)
            return CacheRejectReason::SnapshotNotReady;

        // 拓扑总代数与快照发布时记录的值不一致 ⇒ 成员/绑定/批划分可能已变。
        if (in.snapshot_topology_rev != topology_total)
            return CacheRejectReason::TopologyDirty;

        return CacheRejectReason::Eligible;
    }

    void PrimitiveTopologyLedger::RecordEligibility(const CacheRejectReason reason)
    {
        ++stat_candidate_frames;

        if (reason == CacheRejectReason::Eligible)
        {
            ++stat_eligible_frames;
            return;
        }

        if (reason < CacheRejectReason::Count)
            ++stat_reject[RejectSlot(reason)];
    }

    uint64_t PrimitiveTopologyLedger::GetStatReject(const CacheRejectReason reason) const
    {
        return (reason < CacheRejectReason::Count) ? stat_reject[RejectSlot(reason)] : 0;
    }

    void PrimitiveTopologyLedger::Reset()
    {
        for (size_t i = 0; i < kTopoCount; ++i)
        {
            topology_rev[i]      = 0;
            topology_last_frame[i] = 0;
        }

        topology_total = 0;

        for (size_t i = 0; i < kFactCount; ++i)
            fact_rev[i] = 0;

        fact_total = 0;

        for (size_t i = 0; i < kPoolSlotCount; ++i)
        {
            polled_layout[i]  = 0;
            polled_content[i] = 0;
        }

        polled_valid = false;

        last_gpu_cull_engaged       = false;
        last_gpu_cull_engaged_valid = false;

        frame_serial = 0;

        stat_candidate_frames = 0;
        stat_eligible_frames  = 0;

        stat_pool_layout_changes  = 0;
        stat_pool_content_changes = 0;

        for (size_t i = 0; i < kRejectCount; ++i)
            stat_reject[i] = 0;
    }

    const char *GetTopologyChangeName(const TopologyChange kind)
    {
        return (kind < TopologyChange::Count) ? kTopologyNames[TopoSlot(kind)] : "?";
    }

    const char *GetDynamicFactChangeName(const DynamicFactChange kind)
    {
        return (kind < DynamicFactChange::Count) ? kFactNames[FactSlot(kind)] : "?";
    }

    const char *GetCacheRejectReasonName(const CacheRejectReason reason)
    {
        return (reason < CacheRejectReason::Count) ? kRejectNames[RejectSlot(reason)] : "?";
    }
}//namespace hgl::ecs
