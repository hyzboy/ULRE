#pragma once

#include<hgl/graph/ubo/ModelExpansionArena.h>
#include<hgl/math/VectorTypes.h>      // math::Vector4u（= glm::uvec4，与 GLSL uvec4 同型）
#include<hgl/type/ValueArray.h>

namespace hgl::ecs
{
    /**
     * 模型展开的两张 CPU 侧路由表（GPU 剔除系统私有）。
     *
     * 一级表：`实体 → 组表偏移`。写下标 = `pass * kModelExpansionBucketBaseRowsPerPass + 实体下标`；
     *   写值 = `pass * kModelExpansionGroupBucketSlotsPerPass + 组表偏移`（**加 pass 分区起点**，
     *   展开侧直接用它索引二级表）。
     * 二级表：`组表偏移 + part.shading_group → 组块内首桶号`（+ 该组的有效材质/纹理），
     *   下标 = `pass * kModelExpansionGroupBucketSlotsPerPass + 偏移 + 组号`。
     *
     * **这两条下标/偏移规则只在本类里存在**（`GPUCullSystem` 与批装配只经本类的具名入口
     * 读写）：分区与偏移的唯一权威来源，避免同一算术在写入侧与上传侧各写一遍而漂移。
     *
     * 容量取自 `ModelExpansionArena.h` 的单源常量；越界一律 fail-fast（不静默截断）。
     */
    class ModelGroupRoutingTables
    {
    public:
        /// 按 pass 分份的遍数（与 `RenderPassSlot::Count` / GLSL `ULRE_ARENA_PASSES` 同值）
        static constexpr uint32_t kPasses = 2;

    private:
        /// 实体桶组基址表（CPU 主副本；整表随竞技场逐帧上传）
        ValueArray<uint32_t> bucket_bases;
        /// 二级 shading_group 组基址/路由表（CPU 主副本；整表随竞技场逐帧上传）
        ValueArray<math::Vector4u> group_routes;
        /// 每个 pass 的组表分配游标（下一可用槽）
        uint32_t group_slot_next[kPasses] = {};

    public:
        /// 建表：两张表各按容量填哨兵，游标归零
        bool Create();

        /// 释放（Shutdown 用）
        void Release()
        {
            bucket_bases.ClearAndDeallocate();
            group_routes.ClearAndDeallocate();

            for (uint32_t p = 0; p < kPasses; ++p)
                group_slot_next[p] = 0u;
        }

        /// 一级表：登记实体的组表偏移（`group_table_offset` 是 pass 内偏移，非绝对下标）
        void SetEntityGroupOffset(uint32_t pass_index, uint32_t entity_index, uint32_t group_table_offset);

        /// 一级表：清空本 pass 那半块（回哨兵）。两个 pass 同帧先后装配 ⇒ 只清自己那半。
        void ResetPassEntityBases(uint32_t pass_index);

        /// 二级表：按 (pass, 组数) 申请连续槽并回填哨兵。越界返回 false（调用方 fail-fast）。
        bool AllocateGroupSlots(uint32_t pass_index, uint32_t group_count, uint32_t &out_offset);

        /// 二级表：回填某组的首桶号与有效材质/纹理
        void SetGroupRoute(uint32_t pass_index, uint32_t offset, uint32_t group_index,
                           uint32_t bucket_base,
                           uint32_t material_id = 0, uint32_t texture_id = 0,
                           bool has_material = false);

        void ResetPassGroupRoutes(uint32_t pass_index);
        void ResetAllGroupRoutes();

        /// 上传侧只读视图（未建表 ⇒ nullptr）
        const uint32_t *BucketBaseData() const { return bucket_bases.GetData(); }
        const math::Vector4u *GroupRouteData() const { return group_routes.GetData(); }
    };
}