#include <hgl/ecs/support/DrawItemCompaction.h>
#include <hgl/ecs/support/DrawItemIDStorage.h>

namespace hgl::ecs
{
    void CompactRenderItemHandles(
        const RenderItemHandle *handles,
        const uint32_t count,
        DrawItemIDStorage *id_storage,
        hgl::ValueArray<CompactedDrawRange> &out_ranges,
        CompactionStats *out_stats,
        const uint32_t min_contiguous_run)
    {
        if (out_stats)
        {
            *out_stats = CompactionStats{};
            out_stats->total_input_items = count;
        }

        if (!handles || count == 0)
            return;

        if (count == 1)
        {
            CompactedDrawRange range{};
            range.first_instance = handles[0];
            range.instance_count = 1;
            range.is_direct = true;
            out_ranges.Add(range);

            if (out_stats)
            {
                out_stats->direct_ranges = 1;
                out_stats->direct_items = 1;
                // 1 项全量传统占用 12 字节 (4B L2W + 8B MtlAddr)，直通 0 字节
                out_stats->bytes_saved_over_full = 12;
            }
            return;
        }

        hgl::ValueArray<uint32_t> pending_scattered;
        pending_scattered.Reserve(count);

        auto flush_pending_scattered = [&]()
        {
            const uint32_t scattered_count = static_cast<uint32_t>(pending_scattered.GetCount());
            if (scattered_count == 0)
                return;

            if (id_storage && scattered_count > 1)
            {
                // 多项离散且有二级缓冲存储器：打包写入二级索引表以合并为一个 DrawCall
                const uint32_t offset = id_storage->Append(pending_scattered.GetData(), scattered_count);
                CompactedDrawRange range{};
                range.first_instance = offset | kRenderItemIndexedFlag;
                range.instance_count = scattered_count;
                range.is_direct = false;
                out_ranges.Add(range);

                if (out_stats)
                {
                    out_stats->indexed_ranges += 1;
                    out_stats->indexed_items += scattered_count;
                }
            }
            else
            {
                // 无二级缓冲或仅单项：直接作为单项直通区间下发
                for (uint32_t k = 0; k < scattered_count; ++k)
                {
                    CompactedDrawRange range{};
                    range.first_instance = pending_scattered[k];
                    range.instance_count = 1;
                    range.is_direct = true;
                    out_ranges.Add(range);

                    if (out_stats)
                    {
                        out_stats->direct_ranges += 1;
                        out_stats->direct_items += 1;
                    }
                }
            }

            pending_scattered.Clear();
        };

        const uint32_t effective_min_run = min_contiguous_run > 0 ? min_contiguous_run : 1;

        uint32_t i = 0;
        while (i < count)
        {
            uint32_t run_end = i;
            while (run_end + 1 < count && handles[run_end + 1] == handles[run_end] + 1)
            {
                ++run_end;
            }

            const uint32_t run_len = run_end - i + 1;

            if (run_len >= effective_min_run)
            {
                // 先刷出前面挂起的离散项
                flush_pending_scattered();

                // 输出连号直通区间
                CompactedDrawRange range{};
                range.first_instance = handles[i];
                range.instance_count = run_len;
                range.is_direct = true;
                out_ranges.Add(range);

                if (out_stats)
                {
                    out_stats->direct_ranges += 1;
                    out_stats->direct_items += run_len;
                }

                i = run_end + 1;
            }
            else
            {
                // 长度不足阈值的单项加入散乱缓冲
                for (uint32_t k = i; k <= run_end; ++k)
                {
                    pending_scattered.Add(handles[k]);
                }
                i = run_end + 1;
            }
        }

        // 刷出尾部残留的离散项
        flush_pending_scattered();

        if (out_stats)
        {
            // 传统每帧上传: 每图元 12 字节 (4B L2W + 8B MtlAddr)
            const uint32_t full_legacy_bytes = count * 12;
            // 新架构上传: 直通项 0 字节，索引项 4 字节
            const uint32_t new_uploaded_bytes = out_stats->indexed_items * sizeof(uint32_t);
            out_stats->bytes_saved_over_full = (full_legacy_bytes >= new_uploaded_bytes)
                ? (full_legacy_bytes - new_uploaded_bytes) : 0;
        }
    }
}
