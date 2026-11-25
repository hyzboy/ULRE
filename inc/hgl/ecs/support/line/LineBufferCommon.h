#pragma once

/**
 * LineBufferCommon.h —— Line（宽线）与 SimpleLine（原生 1px）两套线缓冲管线**真正同构**的底层。
 *
 * 只收拢与「顶点流数量」无关的部分：
 *   · 扩容目标容量的算术（granule 取整 + 至少翻倍 + 上限夹取）；
 *   · 扩容失败的同容量退避策略；
 *   · 候选资源（Geometry / GeometryDataBuffer / GeometryDrawRange）的先建后换；
 *   · 多流游标锁步回退。
 *
 * 明确**不**收拢：顶点 ABI（Line 额外一条 Size 宽度流）、着色器领域
 * （`VertexPalette` / `VertexPaletteLines`）、渲染拓扑、Pass 注册与 `Draw` 入口。
 * 两条管线可同时启用，各自的 `LineBuffer` 类型与 `AddSegment` 语义保持独立。
 */

#include <hgl/type/String.h>
#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class VulkanDevice;
        class Geometry;
        class GeometryVertexFormat;
        struct GeometryDataBuffer;
        struct GeometryDrawRange;
    }
}

namespace hgl::ecs::detail
{
    /// 线段缓冲容量上限：`CreateVAB` 内部以 uint32_t 计算 stride * vertex_count。
    inline constexpr uint64_t kLineBufferMaxCapacity = 0xFFFFFFFFu / (2u * 3u * sizeof(float));

    /**
     * @brief 计算扩容目标容量：granule 向上取整，且至少为当前容量的两倍。
     * @param needed            本帧需要的线段数
     * @param granule           容量粒度（两条管线的 LINES_GRANULE）
     * @param current_capacity  当前已分配容量（0 表示尚未分配）
     * @param out_capacity      目标容量（行数）
     * @return false 表示超过 kLineBufferMaxCapacity，调用方应记录错误并放弃扩容
     */
    bool ComputeLineBufferTargetCapacity(uint64_t needed,
                                         uint32_t granule,
                                         uint32_t current_capacity,
                                         uint32_t &out_capacity);

    /**
     * @brief 扩容失败退避：同一目标容量重复失败时按 1 → 2 → 4 … 递增等待，上限 60 帧。
     */
    struct LineBufferBackoff
    {
        uint32_t failed_capacity    = 0;
        uint32_t retry_delay_frames = 0;
        uint32_t last_failure_frame = 0;

        void Reset();

        /// 同目标容量且仍在等待窗口内 ⇒ 本帧放弃重试
        bool ShouldSkip(uint32_t target_capacity, uint32_t frame_serial) const;

        /// 记录一次失败并推进退避窗口
        void RecordFailure(uint32_t target_capacity, uint32_t frame_serial);
    };

    /**
     * @brief 多流游标锁步回退：任一 `Seek` 失败 ⇒ 返回 false。
     * 全部视图都会被尝试（与逐项 `&` 求值等价），不会因前一个失败而跳过后面的流。
     */
    template<typename... Views>
    inline bool SeekAllViews(uint32_t cursor, Views &... views)
    {
        bool ok = true;
        ((ok = views.Seek(cursor) && ok), ...);
        return ok;
    }

    /**
     * @brief 扩容候补资源：先全部建好再一次性替换。
     * 任一环节失败时整体丢弃，绝不触碰在用缓冲。
     */
    struct LineBufferCandidate
    {
        graph::Geometry           *geometry    = nullptr;
        graph::GeometryDataBuffer *data_buffer = nullptr;
        graph::GeometryDrawRange  *draw_range  = nullptr;

        /// 按 `capacity_lines` 行（= `capacity_lines * 2` 顶点）建立候补三元组
        /// @param log_tag 失败日志前缀（`"[LineRenderPipeline]"` / `"[SimpleLineRenderPipeline]"`）
        bool Create(graph::VulkanDevice *dev,
                    const graph::GeometryVertexFormat &format,
                    const AnsiString &name,
                    uint32_t capacity_lines,
                    const char *log_tag);

        void Discard();

        /// 把候补移交给调用方（候补自身置空）。旧资源的退休由调用方在此之前完成。
        void Install(graph::Geometry *&out_geometry,
                     graph::GeometryDataBuffer *&out_data_buffer,
                     graph::GeometryDrawRange *&out_draw_range);
    };
}//namespace hgl::ecs::detail
