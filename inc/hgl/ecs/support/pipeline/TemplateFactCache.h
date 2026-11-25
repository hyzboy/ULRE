#pragma once

#include<hgl/ecs/support/storage/PrimitiveTopologyLedger.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/type/ValueArray.h>

namespace hgl
{
    namespace graph
    {
        class GlobalSSBOBufferRegistry;
    }
}

namespace hgl::ecs
{
    /**
     * 模板部件事实缓存（批管线私有）。
     *
     * 展开归组循环要逐项读 `ModelTemplateRow` 的 `part_offset` / `part_count` /
     * `part_bucket_count`，而这些行落在**映射的 GPU 缓冲**（host-visible、非缓存内存）上，
     * 单字段读约 2.3µs；1000 项棋盘实测该循环占 guard 的 58%（≈1.65ms）。
     * 模板数 ≪ 实例数 ⇒ 按 `template_id` 记忆化，每帧成本从「项数 × 3 次映射读」降为
     * 「模板数 × 一次建表 + 项数 × 哈希查」。
     *
     * 失效：`(拓扑总代数, 池内容变更计数)` 二元组变化 ⇒ 整表清空。两者合起来覆盖
     * 「模板行被改写」「部件行被改写」「池行被回收复用」的全部形态。
     * 无效模板行**不入缓存**（被写成有效后下一帧自然重建）。
     */
    class TemplateFactCache
    {
    public:
        struct Facts
        {
            uint32_t part_offset         = 0;
            uint32_t part_count          = 0;
            uint32_t part_bucket_count   = 0;
            uint32_t shading_group_count = 1;
            ValueArray<uint32_t> slot_count;      ///< 槽 → 该槽部件数
            ValueArray<uint32_t> slot_geometry;   ///< 槽 → geometry_id（该槽桶的几何体）
        };

        /// 解析失败形态（调用方据此保持与原实现逐字相同的拒批原因）
        enum class Result : uint8_t
        {
            Ok = 0,
            RowMissing,    ///< 模板行缺失 / 无部件桶
            PartMissing,   ///< 模板 part 区间越界（部件行缺失）
        };

    private:
        bool     valid            = false;
        uint64_t topology_rev     = 0;
        uint64_t pool_content_rev = 0;

        uint64_t hits   = 0;
        uint64_t misses = 0;
        uint64_t builds = 0;

        UnorderedMap<uint32_t, Facts> facts;

    public:
        /// 每帧一次：代数变了才整表清空
        void Refresh(const PrimitiveTopologyLedger &ledger);

        /// 查缓存，未命中则从池行建表并记忆化；失败返回 nullptr 并置 result
        const Facts *Resolve(graph::GlobalSSBOBufferRegistry *pool,
                             uint32_t template_id,
                             Result &result);

        uint64_t Hits()   const { return hits; }
        uint64_t Misses() const { return misses; }
        uint64_t Builds() const { return builds; }
    };
}