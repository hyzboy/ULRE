#pragma once

#include<cstdint>
#include<vector>
#include<algorithm>

namespace hgl::ecs
{
    /**
     * 行脏位 + 「超容量只报一次」告警闩。
     *
     * `EntityGPUTable` / `EntityRenderIDTable` / `LightRowTable` 三张世界行表此前各自复制了一份
     * **逐字相同**的机制（`row_dirty` + `overflow_reported` + `MarkDirty` / `CollectDirtyRows` /
     * `ClearDirty` / `GetDirtyCount`，以及整表复位里的 `fill(...,0)`）⇒ 抽成这个值类型，
     * 三张表按**组合**持有（成员 `DirtyRowSet dirty;`）。
     *
     * 刻意**不**放进来的部分（各表不同）：
     *   · 告警文案 —— 各表 `ReportOverflow` 自己的 `GLogError` 串；本类只提供一次性闩
     *     （`ArmOverflowWarning()`）。
     *   · 复位容量口径 —— `Reset(kCapacity / kRowStride)` 由调用方给；光源表复位后还要把
     *     主光行标脏，也由调用方补。
     *
     * 语义与改造前逐字一致：`MarkDirty` 仍是「按需扩到行数再判界」，`GetDirtyCount` 仍是逐位计数，
     * `Shift` 行数（`kRowStride`）与行号（`PassBase(pass)+index`）的算法留在各表。
     */
    class DirtyRowSet
    {
    private:

        std::vector<uint8_t> bits;                      ///< 行脏位（1 B/行；上传后清）
        bool                 overflow_reported = false; ///< 超容量告警闩（每表只报一次）

    public:

        /// 按行数复位（全 0）；行数变化时同时扩到该行数
        void Reset(size_t rows) { bits.assign(rows,0); }

        /// 标脏：行数不足先扩到 `row_count`，再按 `index` 判界（与原实现同语义）
        void MarkDirty(uint32_t index,size_t row_count)
        {
            if (bits.size() < row_count)
                bits.resize(row_count,0);

            if (index < bits.size())
                bits[index] = 1;
        }

        /// 收集脏行下标（不改状态）；`out` 会被清空后填充
        void CollectDirtyRows(std::vector<uint32_t> &out) const
        {
            out.clear();

            for (size_t i = 0; i < bits.size(); ++i)
                if (bits[i])
                    out.push_back(static_cast<uint32_t>(i));
        }

        /// 上传完成后清脏（唯一入口：世界的上传路径）
        void ClearDirty()
        {
            if (!bits.empty())
                std::fill(bits.begin(),bits.end(),0);
        }

        uint32_t GetDirtyCount() const
        {
            uint32_t n = 0;

            for (uint8_t d : bits)
                n += d ? 1u : 0u;

            return n;
        }

        /// 「超容量只报一次」闩：返回 true = 本次是第一次 ⇒ 调用方接着打自己的告警文案
        bool ArmOverflowWarning()
        {
            if (overflow_reported)
                return false;

            overflow_reported = true;
            return true;
        }
    };
}//namespace hgl::ecs
