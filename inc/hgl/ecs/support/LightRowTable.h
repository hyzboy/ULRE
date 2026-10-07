#pragma once

#include<hgl/graph/ubo/LightRow.h>
#include<hgl/log/Log.h>
#include<algorithm>
#include<cstdint>
#include<cstring>
#include<vector>

namespace hgl
{
    namespace ecs
    {
        /**
         * LightRowTable —— **光源行表**（`graph::LightRow` 行数组）的世界私有存储（T6b）。
         *
         * 设计依据（`doc/future/lighting-shadow-domain-split-plan.md` §2/§3）：
         *   · **行 0 = 主方向光**（`graph::kMainLightRowIndex`）：阴影域与 shader 都按这条假设取光；
         *   · 光源定义是**世界私有**（D2）—— 不跟 `EnvProfile`（profile 可被多世界共享 ⇒ 互踩：
         *     影子数据当年挂 profile、CSM 资源挂世界，两个世界共用 `kEnvProfileDefault` 时就互相覆写）；
         *   · 容量 `graph::kLightRowCapacity`（拍板冻结 16；超限 = **fail-fast**，不扩容、不静默截断）；
         *   · GPU 侧是它的**设备镜像**（按帧槽切份 + 脏行上传，与 `EntityGPUTable` 同范式）：
         *     光参数可由作者逐帧改（示例里太阳方向就是逐帧写的）⇒ 行内容随帧变 ⇒ 必须按帧槽分份，
         *     单份表会被本帧写入覆盖上一帧正在被读的内容（跨帧别名）。
         *
         * ⚠ 写者唯一化：只有 `LightSystem` 的编辑入口（+ 建表期的行 0 默认主光）写它；
         *   `ECSContext` 只负责「脏行 → 设备镜像」上传与地址发布（`WorldAddresses.addr_lights`）。
         */
        class LightRowTable
        {
        public:

            /// 容量（`graph::kLightRowCapacity` 冻结值；超限 = 项目 bug）
            static constexpr uint32_t kCapacity = graph::kLightRowCapacity;

        private:

            std::vector<graph::LightRow> rows;        ///< 按行号直索引（行 0 = 主方向光）
            std::vector<uint8_t>         row_dirty;   ///< 行脏位（1 B/行；上传后清）
            bool                         overflow_reported = false;

            void MarkDirty(uint32_t index)
            {
                if (row_dirty.size() < rows.size())
                    row_dirty.resize(rows.size(), 0);

                if (index < row_dirty.size())
                    row_dirty[index] = 1;
            }

            void ReportOverflow(uint32_t index)
            {
                if (overflow_reported)
                    return;

                overflow_reported = true;

                // fail-fast：容量与场景规模不匹配是**项目 bug**（冻结值、不扩容、不静默截断）
                GLogError("[LightRowTable] 光源行表超容量：row=%u ≥ %u ⇒ 该光源不进表"
                          "（容量是拍板冻结值，超限=项目 bug；不扩容、不静默截断）",
                          index, kCapacity);
            }

        public:

            bool IsAllocated() const { return !rows.empty(); }

            /// 懒分配（16 行 × 64 B = 1 KB；世界从未点灯则一分不占）。
            /// 分配即把**行 0 = 默认主方向光**落进表：主光恒存在（不依赖 `LightSystem` 是否注册），
            /// 否则「世界有渲染、没有被启用的光源」= 全黑，而这不会报任何错。
            void Ensure()
            {
                if (!rows.empty())
                    return;

                rows.assign(kCapacity, graph::LightRow{});
                row_dirty.assign(kCapacity, 0);

                rows[graph::kMainLightRowIndex] = graph::MakeDefaultMainLight();
                row_dirty[graph::kMainLightRowIndex] = 1;
            }

            const graph::LightRow *Row(uint32_t index) const
            {
                return index < rows.size() ? &rows[index] : nullptr;
            }

            graph::LightRow *Row(uint32_t index)
            {
                return index < rows.size() ? &rows[index] : nullptr;
            }

        public: // ── 写入路径（唯一写者 = LightSystem 的编辑入口）──

            /// 写一行（**值变才标脏**：光参数在多数帧里不变 ⇒ 无条件标脏 = 每帧整表重传，
            /// 脏行上传的意义会被吃掉；与 `EntityGPUTable::SetTransformRow` 同纪律）。
            void SetRow(uint32_t index, const graph::LightRow &row)
            {
                Ensure();

                graph::LightRow *dst = Row(index);

                if (!dst)
                {
                    ReportOverflow(index);
                    return;
                }

                if (std::memcmp(dst, &row, sizeof(graph::LightRow)) == 0)
                    return;

                *dst = row;
                MarkDirty(index);
            }

            void Clear()
            {
                for (graph::LightRow &row : rows)
                    row = graph::LightRow{};

                if (!row_dirty.empty())
                    std::fill(row_dirty.begin(), row_dirty.end(), 0);
            }

        public: // ── 上传计划（与 EntityGPUTable 同形）：把**脏行**交出去 ──

            /// 收集脏行下标（不改状态）；`out` 会被清空后填充
            void CollectDirtyRows(std::vector<uint32_t> &out) const
            {
                out.clear();

                for (uint32_t i = 0; i < row_dirty.size(); ++i)
                    if (row_dirty[i])
                        out.push_back(i);
            }

            /// 上传完成后清脏（唯一入口：世界的上传路径）
            void ClearDirty()
            {
                if (!row_dirty.empty())
                    std::fill(row_dirty.begin(), row_dirty.end(), 0);
            }

            uint32_t GetDirtyCount() const
            {
                uint32_t n = 0;

                for (uint8_t d : row_dirty)
                    n += d ? 1u : 0u;

                return n;
            }
        };
    }//namespace ecs
}//namespace hgl
