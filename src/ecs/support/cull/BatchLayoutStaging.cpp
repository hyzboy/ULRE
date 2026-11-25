#include <hgl/ecs/support/cull/BatchLayoutStaging.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    void BatchLayoutStaging::ResetPass(const uint32_t pass_index)
    {
        if (pass_index >= kPasses)
            return;

        for (uint32_t i = 0; i < kBlockStride; ++i)
            rows[BlockBase(pass_index) + i] = Row{};
    }

    void BatchLayoutStaging::SetLayout(const uint32_t pass_index, const uint32_t batch_index,
                                       const uint32_t row_base, const uint32_t capacity,
                                       const uint32_t topology, const uint32_t group_size)
    {
        if (pass_index >= kPasses)
        {
            GLogError(u8"[BatchLayoutStaging] pass 号越界：%u ≥ kPasses(%u)（fail-fast）",
                      pass_index, kPasses);
            return;
        }

        if (batch_index >= kMaxBatches)
        {
            GLogError(u8"[BatchLayoutStaging] 批号越界：%u ≥ kMaxBatches(%u)（超限 = 项目 bug，fail-fast）",
                      batch_index, kMaxBatches);
            return;
        }

        if (!reported_misaligned_rows && (row_base % kSliceRowAlignment) != 0u)
        {
            reported_misaligned_rows = true;
            GLogError(u8"[BatchLayoutStaging] 切片起点未 %u 行对齐：row_base=%u（契约 §9 M4）"
                      u8"⇒ 三表 push 地址会违 16B 对齐", kSliceRowAlignment, row_base);
        }

        Row &row = rows[BlockBase(pass_index) + batch_index];
        row.row_base   = row_base;
        row.capacity   = capacity;
        row.topology   = topology;
        row.group_size = group_size;
    }

    bool BatchLayoutStaging::Commit(const uint32_t pass_index, const uint32_t batch_count)
    {
        if (pass_index >= kPasses)
        {
            GLogError(u8"[BatchLayoutStaging] cannot finish batch layout: pass=%u", pass_index);
            return false;
        }

        if (batch_count > kMaxBatches)
        {
            GLogError(u8"[BatchLayoutStaging] 批数越界：%u > kMaxBatches(%u) ⇒ 拒绝写入",
                      batch_count, kMaxBatches);
            counts[pass_index] = 0;
            return false;
        }

        counts[pass_index] = batch_count;
        return true;
    }
}