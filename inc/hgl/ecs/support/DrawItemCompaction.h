#pragma once

#include <hgl/type/ValueArray.h>
#include <hgl/graph/render/RenderItemDescriptor.h>
#include <cstdint>

namespace hgl::ecs
{
    using graph::RenderItemHandle;
    using graph::INVALID_RENDER_ITEM_HANDLE;

    class DrawItemIDStorage;

    /// 标记首个实例索引为二级索引表模式的最高位掩码
    constexpr uint32_t kRenderItemIndexedFlag = 0x80000000u;

    /// 判断 draw command 中的 first_instance 是否为二级索引表模式
    inline bool IsIndexedDraw(const uint32_t first_instance)
    {
        return (first_instance & kRenderItemIndexedFlag) != 0;
    }

    /// 提取二级索引表中的真实偏移量 (offset)
    inline uint32_t GetDrawIndexOffset(const uint32_t first_instance)
    {
        return first_instance & ~kRenderItemIndexedFlag;
    }

    /**
     * @brief 经连号区间折叠后的绘制批次范围
     */
    struct CompactedDrawRange
    {
        uint32_t first_instance = 0;   ///< 直通模式: 起始 Handle；索引模式: (offset | kRenderItemIndexedFlag)
        uint32_t instance_count = 0;   ///< 实例数量
        bool     is_direct      = true;///< 是否为连号直通模式 (true=直通, false=二级索引)

        bool operator==(const CompactedDrawRange &rhs) const = default;
    };

    /**
     * @brief 连号区间折叠与离散 Handle 打包统计信息
     */
    struct CompactionStats
    {
        uint32_t total_input_items     = 0; ///< 输入图元项总数
        uint32_t direct_ranges         = 0; ///< 连号直通区间数量
        uint32_t direct_items          = 0; ///< 连号直通图元总数
        uint32_t indexed_ranges        = 0; ///< 二级索引区间数量
        uint32_t indexed_items         = 0; ///< 二级索引图元总数
        uint32_t bytes_saved_over_full = 0; ///< 相比全量上传节省的字节数
    };

    /**
     * @brief 对一批 RenderItemHandle 执行连号区间折叠算法 (Run-Length Compaction)
     * 
     * @param handles 输入的 Handle 列表（建议预先按升序排序）
     * @param count Handle 数量
     * @param id_storage 可选的二级索引表存储器（若为 nullptr 则强制全量直通拆分为单项）
     * @param out_ranges 输出折叠后的绘制区间列表
     * @param out_stats 可选的统计输出
     * @param min_contiguous_run 认定为直通连续区间的最小长度（默认 2）
     */
    void CompactRenderItemHandles(
        const RenderItemHandle *handles,
        const uint32_t count,
        DrawItemIDStorage *id_storage,
        hgl::ValueArray<CompactedDrawRange> &out_ranges,
        CompactionStats *out_stats = nullptr,
        const uint32_t min_contiguous_run = 2);
}
