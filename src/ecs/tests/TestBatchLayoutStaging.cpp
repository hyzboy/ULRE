#include <hgl/ecs/support/cull/BatchLayoutStaging.h>
#include <hgl/log/Log.h>

using namespace hgl;
using namespace hgl::ecs;

int main()
{
    {
        BatchLayoutStaging s;

        // 初值：两个 pass 都无批，行视图拿不到东西
        if (s.BatchCount(0) != 0u || s.BatchCount(1) != 0u)
        {
            GLogError(u8"BatchLayoutStaging: 初值批数必须为 0");
            return 1;
        }

        if (s.RowAt(0, 0) != nullptr)
        {
            GLogError(u8"BatchLayoutStaging: 无批时行视图必须为空");
            return 2;
        }

        GLogInfo(u8"BatchLayoutStaging: 初值通过");
    }

    {
        BatchLayoutStaging s;

        s.ResetPass(0);
        s.SetLayout(0, 0, 0u, 12u, BatchLayoutStaging::kTopologyTriangles, 64u);
        s.SetLayout(0, 1, 16u, 7u, BatchLayoutStaging::kTopologyLines, 1u);

        if (!s.Commit(0, 2u))
        {
            GLogError(u8"BatchLayoutStaging: 容量内提交必须成功");
            return 3;
        }

        const auto *r0 = s.RowAt(0, 0);
        const auto *r1 = s.RowAt(0, 1);

        if (!r0 || !r1
            || r0->row_base != 0u || r0->capacity != 12u
            || r0->topology != BatchLayoutStaging::kTopologyTriangles || r0->group_size != 64u)
        {
            GLogError(u8"BatchLayoutStaging: 第 0 行字段未按序写入");
            return 4;
        }

        if (r1->row_base != 16u || r1->capacity != 7u
            || r1->topology != BatchLayoutStaging::kTopologyLines || r1->group_size != 1u)
        {
            GLogError(u8"BatchLayoutStaging: 第 1 行字段未按序写入");
            return 5;
        }

        // 批数之外的批号不可见
        if (s.RowAt(0, 2) != nullptr)
        {
            GLogError(u8"BatchLayoutStaging: 超出本帧批数的行必须不可见");
            return 6;
        }

        GLogInfo(u8"BatchLayoutStaging: 行字段写入与批数可见性通过");
    }

    {
        BatchLayoutStaging s;

        // pass 隔离：块下标 = pass * kBlockStride，同批号在两个 pass 不互相覆盖
        s.ResetPass(0);
        s.ResetPass(1);
        s.SetLayout(0, 0, 0u, 3u, BatchLayoutStaging::kTopologyTriangles, 64u);
        s.SetLayout(1, 0, 32u, 9u, BatchLayoutStaging::kTopologyLines, 1u);
        s.Commit(0, 1u);
        s.Commit(1, 1u);

        if (s.RowAt(0, 0)->capacity != 3u || s.RowAt(1, 0)->capacity != 9u)
        {
            GLogError(u8"BatchLayoutStaging: 两个 pass 的同批号必须各自独立");
            return 7;
        }

        // 只复位一个 pass（清行、不动批数），另一个不受影响
        s.ResetPass(0);

        if (s.RowAt(0, 0) == nullptr || s.RowAt(0, 0)->capacity != 0u || s.RowAt(0, 0)->row_base != 0u)
        {
            GLogError(u8"BatchLayoutStaging: ResetPass 必须把本 pass 的行清成零");
            return 8;
        }

        if (s.RowAt(1, 0) == nullptr || s.RowAt(1, 0)->capacity != 9u)
        {
            GLogError(u8"BatchLayoutStaging: ResetPass 不得影响另一个 pass");
            return 9;
        }

        GLogInfo(u8"BatchLayoutStaging: pass 隔离与单 pass 复位通过");
    }

    {
        BatchLayoutStaging s;

        // 容量边界：恰好 kMaxBatches 成功，多一个必须拒绝并把批数清零
        s.ResetPass(0);
        if (!s.Commit(0, BatchLayoutStaging::kMaxBatches))
        {
            GLogError(u8"BatchLayoutStaging: 恰好等于上限必须成功");
            return 10;
        }

        if (s.BatchCount(0) != BatchLayoutStaging::kMaxBatches)
        {
            GLogError(u8"BatchLayoutStaging: 提交后批数应为上限值");
            return 11;
        }

        if (s.Commit(0, BatchLayoutStaging::kMaxBatches + 1u))
        {
            GLogError(u8"BatchLayoutStaging: 超上限必须拒绝");
            return 12;
        }

        if (s.BatchCount(0) != 0u)
        {
            GLogError(u8"BatchLayoutStaging: 拒绝后必须把本 pass 批数清零");
            return 13;
        }

        if (!BatchLayoutStaging::IsValidBatchCount(BatchLayoutStaging::kMaxBatches)
         || BatchLayoutStaging::IsValidBatchCount(BatchLayoutStaging::kMaxBatches + 1u))
        {
            GLogError(u8"BatchLayoutStaging: IsValidBatchCount 边界判定错误");
            return 14;
        }

        GLogInfo(u8"BatchLayoutStaging: 容量边界通过");
    }

    {
        BatchLayoutStaging s;

        // 批号越界：静默拒绝，不得越界写（写进去会让别的 pass 的行被污染）
        s.ResetPass(0);
        s.SetLayout(0, BatchLayoutStaging::kMaxBatches, 0u, 1u, 0u, 1u);

        if (!s.Commit(0, 1u))
        {
            GLogError(u8"BatchLayoutStaging: 前置提交应成功");
            return 15;
        }

        const auto *r = s.RowAt(0, 0);
        if (!r || r->capacity != 0u)
        {
            GLogError(u8"BatchLayoutStaging: 越界批号的写入必须被丢弃");
            return 16;
        }

        GLogInfo(u8"BatchLayoutStaging: 批号越界丢弃通过");
    }

    {
        BatchLayoutStaging s;

        // 整表数据视图：两块共 kPasses * kBlockStride 行
        if (BatchLayoutStaging::DataBytes()
            != size_t(BatchLayoutStaging::kPasses)
             * size_t(BatchLayoutStaging::kBlockStride)
             * sizeof(BatchLayoutStaging::Row))
        {
            GLogError(u8"BatchLayoutStaging: DataBytes 与行列数不一致");
            return 17;
        }

        if (s.Data() == nullptr)
        {
            GLogError(u8"BatchLayoutStaging: Data 不得为空");
            return 18;
        }

        GLogInfo(u8"BatchLayoutStaging: 整表数据视图通过");
    }

    GLogInfo(u8"=== All BatchLayoutStaging tests passed successfully! ===");
    return 0;
}