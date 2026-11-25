#include <hgl/ecs/support/cull/ModelGroupRoutingTables.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    bool ModelGroupRoutingTables::Create()
    {
        bucket_bases.ClearAndDeallocate();
        bucket_bases.Reserve(int(graph::kModelExpansionMaxBucketBaseRows));

        for (uint32_t r = 0; r < graph::kModelExpansionMaxBucketBaseRows; ++r)
            bucket_bases.Add(graph::kModelExpansionInvalidBucketBase);

        group_routes.ClearAndDeallocate();
        group_routes.Reserve(int(graph::kModelExpansionMaxGroupBucketSlots));

        for (uint32_t r = 0; r < graph::kModelExpansionMaxGroupBucketSlots; ++r)
            group_routes.Add({graph::kModelExpansionInvalidBucketBase, 0u, 0u, 0u});

        for (uint32_t p = 0; p < kPasses; ++p)
            group_slot_next[p] = 0u;

        return bucket_bases.GetData() != nullptr && group_routes.GetData() != nullptr;
    }

    void ModelGroupRoutingTables::SetEntityGroupOffset(const uint32_t pass_index,
                                                       const uint32_t entity_index,
                                                       const uint32_t group_table_offset)
    {
        static bool s_row_warned = false;

        if (pass_index >= kPasses
         || entity_index >= graph::kModelExpansionBucketBaseRowsPerPass
         || group_table_offset >= graph::kModelExpansionGroupBucketSlotsPerPass)
        {
            if (!s_row_warned)
            {
                s_row_warned = true;
                GLogError(u8"[ModelGroupRoutingTables] 组表偏移写入越界：pass=%u(≥%u) 实体=%u(≥%u) 偏移=%u(≥%u) ⇒ 丢弃",
                          pass_index, kPasses,
                          entity_index, graph::kModelExpansionBucketBaseRowsPerPass,
                          group_table_offset, graph::kModelExpansionGroupBucketSlotsPerPass);
            }
            return;
        }

        uint32_t *table = bucket_bases.GetData();

        if (!table)
            return;

        table[size_t(pass_index) * graph::kModelExpansionBucketBaseRowsPerPass + entity_index] =
            pass_index * graph::kModelExpansionGroupBucketSlotsPerPass + group_table_offset;
    }

    void ModelGroupRoutingTables::ResetPassEntityBases(const uint32_t pass_index)
    {
        if (pass_index >= kPasses)
            return;

        uint32_t *table = bucket_bases.GetData();

        if (!table)
            return;

        uint32_t *block = table + size_t(pass_index) * graph::kModelExpansionBucketBaseRowsPerPass;

        for (uint32_t r = 0; r < graph::kModelExpansionBucketBaseRowsPerPass; ++r)
            block[r] = graph::kModelExpansionInvalidBucketBase;
    }

    bool ModelGroupRoutingTables::AllocateGroupSlots(const uint32_t pass_index,
                                                     const uint32_t group_count,
                                                     uint32_t &out_offset)
    {
        out_offset = graph::kModelExpansionInvalidBucketBase;

        if (pass_index >= kPasses || group_count == 0u)
            return false;

        auto *table = group_routes.GetData();

        if (!table)
            return false;

        uint32_t &next = group_slot_next[pass_index];

        if (group_count > graph::kModelExpansionGroupBucketSlotsPerPass - next)
        {
            static bool s_warned = false;

            if (!s_warned)
            {
                s_warned = true;
                GLogError(u8"[ModelGroupRoutingTables] 组基址表槽位耗尽：pass=%u 需要 %u 槽，已用 %u / 容量 %u"
                          u8"（模板/组数超设计上限 ⇒ 该批本帧不接管）",
                          pass_index, group_count, next,
                          graph::kModelExpansionGroupBucketSlotsPerPass);
            }

            return false;
        }

        auto *block = table + size_t(pass_index) * graph::kModelExpansionGroupBucketSlotsPerPass;

        for (uint32_t g = 0; g < group_count; ++g)
            block[next + g] = {graph::kModelExpansionInvalidBucketBase, 0u, 0u, 0u};

        out_offset = next;
        next += group_count;
        return true;
    }

    void ModelGroupRoutingTables::SetGroupRoute(const uint32_t pass_index,
                                                const uint32_t offset,
                                                const uint32_t group_index,
                                                const uint32_t bucket_base,
                                                const uint32_t material_id,
                                                const uint32_t texture_id,
                                                const bool has_material)
    {
        if (pass_index >= kPasses
         || offset >= graph::kModelExpansionGroupBucketSlotsPerPass
         || group_index >= graph::kModelExpansionGroupBucketSlotsPerPass - offset)
        {
            GLogError(u8"[ModelGroupRoutingTables] Invalid group route: pass=%u offset=%u group=%u",
                      pass_index, offset, group_index);
            return;
        }

        auto *table = group_routes.GetData();

        if (!table)
            return;

        table[size_t(pass_index) * graph::kModelExpansionGroupBucketSlotsPerPass
              + offset + group_index] = {bucket_base, material_id, texture_id, has_material ? 1u : 0u};
    }

    void ModelGroupRoutingTables::ResetPassGroupRoutes(const uint32_t pass_index)
    {
        if (pass_index >= kPasses)
            return;

        group_slot_next[pass_index] = 0u;

        auto *table = group_routes.GetData();

        if (!table)
            return;

        auto *block = table + size_t(pass_index) * graph::kModelExpansionGroupBucketSlotsPerPass;

        for (uint32_t r = 0; r < graph::kModelExpansionGroupBucketSlotsPerPass; ++r)
            block[r] = {graph::kModelExpansionInvalidBucketBase, 0u, 0u, 0u};
    }

    void ModelGroupRoutingTables::ResetAllGroupRoutes()
    {
        for (uint32_t p = 0; p < kPasses; ++p)
            ResetPassGroupRoutes(p);
    }
}