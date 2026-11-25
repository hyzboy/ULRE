#include <hgl/ecs/support/cull/ModelGroupRoutingTables.h>
#include <hgl/log/Log.h>

using namespace hgl;
using namespace hgl::ecs;

namespace
{
    constexpr uint32_t kPasses = ModelGroupRoutingTables::kPasses;

    /// 一级表里某实体的登记值（含 pass 分区起点）
    uint32_t EntityBaseAt(const ModelGroupRoutingTables &t, uint32_t pass, uint32_t entity)
    {
        return t.BucketBaseData()[size_t(pass) * graph::kModelExpansionBucketBaseRowsPerPass + entity];
    }

    /// 二级表里某组的首桶号
    uint32_t GroupBaseAt(const ModelGroupRoutingTables &t, uint32_t pass, uint32_t offset, uint32_t group)
    {
        return t.GroupRouteData()[size_t(pass) * graph::kModelExpansionGroupBucketSlotsPerPass
                                  + offset + group].x;
    }
}

int main()
{
    {
        ModelGroupRoutingTables t;
        if (!t.Create() || !t.BucketBaseData() || !t.GroupRouteData())
        {
            GLogError(u8"ModelGroupRoutingTables: Create 必须建出两张表");
            return 1;
        }

        // 建表后整表是哨兵
        if (EntityBaseAt(t, 0, 0) != graph::kModelExpansionInvalidBucketBase
         || EntityBaseAt(t, kPasses - 1, graph::kModelExpansionBucketBaseRowsPerPass - 1)
                != graph::kModelExpansionInvalidBucketBase
         || GroupBaseAt(t, 0, 0, 0) != graph::kModelExpansionInvalidBucketBase)
        {
            GLogError(u8"ModelGroupRoutingTables: 建表后必须全为哨兵");
            return 2;
        }

        GLogInfo(u8"ModelGroupRoutingTables: 建表与哨兵初值通过");
    }

    {
        ModelGroupRoutingTables t;
        t.Create();

        // 一级表：写值必须含 pass 分区起点（展开侧直接索引二级表）
        t.SetEntityGroupOffset(0, 7, 100);
        t.SetEntityGroupOffset(1, 7, 100);

        const uint32_t expect0 = 0u * graph::kModelExpansionGroupBucketSlotsPerPass + 100u;
        const uint32_t expect1 = 1u * graph::kModelExpansionGroupBucketSlotsPerPass + 100u;

        if (EntityBaseAt(t, 0, 7) != expect0 || EntityBaseAt(t, 1, 7) != expect1)
        {
            GLogError(u8"ModelGroupRoutingTables: 一级表写值必须加 pass 分区起点（got %u/%u，want %u/%u）",
                      EntityBaseAt(t, 0, 7), EntityBaseAt(t, 1, 7), expect0, expect1);
            return 3;
        }

        // 只清本 pass 那半块，另一 pass 不受影响
        t.ResetPassEntityBases(0);

        if (EntityBaseAt(t, 0, 7) != graph::kModelExpansionInvalidBucketBase)
        {
            GLogError(u8"ModelGroupRoutingTables: ResetPassEntityBases 必须清掉本 pass 的登记");
            return 4;
        }

        if (EntityBaseAt(t, 1, 7) != expect1)
        {
            GLogError(u8"ModelGroupRoutingTables: ResetPassEntityBases 不得影响另一个 pass");
            return 5;
        }

        GLogInfo(u8"ModelGroupRoutingTables: 一级表分区与半块清理通过");
    }

    {
        ModelGroupRoutingTables t;
        t.Create();

        // 二级表：分配不重叠，且初始为哨兵
        uint32_t off_a = graph::kModelExpansionInvalidBucketBase;
        uint32_t off_b = graph::kModelExpansionInvalidBucketBase;

        if (!t.AllocateGroupSlots(0, 3, off_a) || !t.AllocateGroupSlots(0, 2, off_b))
        {
            GLogError(u8"ModelGroupRoutingTables: 容量内的分配必须成功");
            return 6;
        }

        if (off_a != 0u || off_b != 3u)
        {
            GLogError(u8"ModelGroupRoutingTables: 分配游标必须连续推进（got %u,%u）", off_a, off_b);
            return 7;
        }

        if (GroupBaseAt(t, 0, off_a, 0) != graph::kModelExpansionInvalidBucketBase
         || GroupBaseAt(t, 0, off_b, 1) != graph::kModelExpansionInvalidBucketBase)
        {
            GLogError(u8"ModelGroupRoutingTables: 新分配的槽必须是哨兵");
            return 8;
        }

        // 回填后按 (pass, offset, group) 精确定位
        t.SetGroupRoute(0, off_a, 2, 55, 9, 11, true);

        if (GroupBaseAt(t, 0, off_a, 2) != 55u)
        {
            GLogError(u8"ModelGroupRoutingTables: 组路由未落在 (pass,offset,group) 指定槽");
            return 9;
        }

        // 另一个 pass 的同偏移槽必须仍是哨兵（分区隔离）
        if (GroupBaseAt(t, 1, off_a, 2) != graph::kModelExpansionInvalidBucketBase)
        {
            GLogError(u8"ModelGroupRoutingTables: 组路由必须按 pass 隔离");
            return 10;
        }

        const auto &route = t.GroupRouteData()[size_t(0) * graph::kModelExpansionGroupBucketSlotsPerPass
                                               + off_a + 2];
        if (route.y != 9u || route.z != 11u || route.w != 1u)
        {
            GLogError(u8"ModelGroupRoutingTables: 组路由的材质/纹理/有效位未按序写入");
            return 11;
        }

        GLogInfo(u8"ModelGroupRoutingTables: 二级表分配/回填/pass 隔离通过");
    }

    {
        ModelGroupRoutingTables t;
        t.Create();

        // 容量边界：恰好填满成功，再多一个必须失败（fail-fast，不静默截断）
        const uint32_t full = graph::kModelExpansionGroupBucketSlotsPerPass;
        uint32_t off = 0;
        if (!t.AllocateGroupSlots(0, full, off) || off != 0u)
        {
            GLogError(u8"ModelGroupRoutingTables: 恰好填满容量必须成功");
            return 12;
        }

        uint32_t over = 0;
        if (t.AllocateGroupSlots(0, 1, over))
        {
            GLogError(u8"ModelGroupRoutingTables: 超容量必须失败（不得静默截断）");
            return 13;
        }

        if (over != graph::kModelExpansionInvalidBucketBase)
        {
            GLogError(u8"ModelGroupRoutingTables: 分配失败必须回写哨兵偏移");
            return 14;
        }

        // Reset 后游标归零、表回哨兵
        t.ResetPassGroupRoutes(0);
        if (!t.AllocateGroupSlots(0, 1, off) || off != 0u)
        {
            GLogError(u8"ModelGroupRoutingTables: ResetPassGroupRoutes 必须归还游标");
            return 15;
        }

        GLogInfo(u8"ModelGroupRoutingTables: 容量边界与游标归零通过");
    }

    {
        ModelGroupRoutingTables t;
        t.Create();

        // 越界 pass / 越界实体：静默丢弃，不得踩内存
        t.SetEntityGroupOffset(kPasses, 0, 1);
        t.SetEntityGroupOffset(0, graph::kModelExpansionBucketBaseRowsPerPass, 1);
        t.SetEntityGroupOffset(0, 0, graph::kModelExpansionGroupBucketSlotsPerPass);

        for (uint32_t p = 0; p < kPasses; ++p)
            if (EntityBaseAt(t, p, 0) != graph::kModelExpansionInvalidBucketBase)
            {
                GLogError(u8"ModelGroupRoutingTables: 越界写入必须被丢弃");
                return 16;
            }

        // 越界 pass 的分配必须失败
        uint32_t off = 0;
        if (t.AllocateGroupSlots(kPasses, 1, off))
        {
            GLogError(u8"ModelGroupRoutingTables: 越界 pass 分配必须失败");
            return 17;
        }

        GLogInfo(u8"ModelGroupRoutingTables: 越界写入丢弃通过");
    }

    {
        ModelGroupRoutingTables t;
        t.Create();
        t.Release();

        if (t.BucketBaseData() || t.GroupRouteData())
        {
            GLogError(u8"ModelGroupRoutingTables: Release 后两张表都必须失效");
            return 18;
        }

        GLogInfo(u8"ModelGroupRoutingTables: Release 通过");
    }

    GLogInfo(u8"=== All ModelGroupRoutingTables tests passed successfully! ===");
    return 0;
}