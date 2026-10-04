#pragma once

#include <hgl/ecs/support/EntityGPUTable.h>
#include <hgl/graph/render/RenderItemDescriptor.h>
#include <vector>

namespace hgl
{
    namespace ecs
    {
        /**
         * EntityRenderIDTable —— **实体 4-ID 表**（世界私有，T3d-3a）。
         *
         * 存在理由（`doc/future/entity-rows-gpu-cull-plan.md` §8.2d）：
         *   `EntityGPU.row[Geometry]` 抄的是 `Entity::component_rows[]` = **组件池行号**
         *   （每实体一行），而 AABB / MeshDrawParams 全局池按 **asset geometry_id** 索引
         *   （一号双表，每 asset 一行）⇒ 两者**不同口径**（PBRSpheres 实测：组件行 1..100
         *   vs 池 id 1..10）。GPU 消费者需要池口径 ⇒ 本表把 collect 每帧**已经算好**的
         *   四元组（`transform.GetID()` / 池 `geometry_id` / `data_index_row` / 纹理引用行）
         *   按 `EntityID.index` 发布成 GPU 可达 SSBO —— 行型直接复用 4-ID 终态的
         *   `graph::RenderItemDescriptor`（16 B），**不制造第二个描述符形状**。
         *
         *   · 写者 = `RenderPrimitiveCollectSystem`（唯一现算点，与喂给
         *     `SetRenderItem4ID` 的四元组**逐字同源** ⇒ 无第二口径）；
         *   · 消费者 = GPU 剔除着色器（池 geometry_id 查 AABB）+ CPU 参考剔除（同口径对拍）
         *     + 形态 B gather（material/texture 行 = gap #1/#2 的解）；
         *   · `transform_id` 存 `GetID()` 原始句柄（与 RenderItemDescriptor 同语义）；
         *     **L2W 行号仍走 `EntityGPU.row[Transform]`**（含帧槽段基址，那边已闭环）；
         *   · 按帧槽切份 + 脏行写进每一份（与实体行表 T3c-3 同纪律：可动实体的
         *     `transform_id` 随帧槽变 ⇒ 单份跨帧别名）；值变才标脏 ⇒ 静态实体首帧上行一次。
         *
         * ⚠ 成员声明位置与 `entity_gpu_table` 同批（先于 `entity_manager`）：
         *   `~Entity` 经 `DeactivateEntityRow` 清本表行，表先死会写到已释放内存。
         */
        class EntityRenderIDTable
        {
        public:

            /// 容量与实体行表同口径（P-B8-6 冻结值；同一 index 空间，必须等容）
            static constexpr uint32_t kCapacity = EntityGPUTable::kCapacity;

        private:

            std::vector<graph::RenderItemDescriptor> rows;   ///< 按 `EntityID.index` 直索引
            std::vector<uint8_t> row_dirty;                  ///< 行脏位（1 B/行；上传后清）
            bool overflow_reported = false;

            void MarkDirty(uint32_t index)
            {
                if (row_dirty.size() < rows.size())
                    row_dirty.resize(rows.size(), 0);

                if (index < row_dirty.size())
                    row_dirty[index] = 1;
            }

            void ReportOverflow(const EntityID id)
            {
                if (overflow_reported)
                    return;

                overflow_reported = true;

                // 与实体行表同纪律：预算与场景规模不匹配 = 项目 bug（fail-fast，不扩容）
                GLogError("[EntityRenderIDTable] 实体 4-ID 表超容量：index=%u ≥ %u ⇒ 该实体不进表",
                          id.index, kCapacity);
            }

        public:

            bool IsAllocated() const { return !rows.empty(); }

            /// 懒分配（与实体行表同策略；行 16 B ⇒ 全表 64 KB）
            void Ensure()
            {
                if (rows.empty())
                {
                    rows.resize(kCapacity);
                    row_dirty.assign(kCapacity, 0);
                }
            }

            const graph::RenderItemDescriptor *Row(uint32_t index) const
            {
                return index < rows.size() ? &rows[index] : nullptr;
            }

            graph::RenderItemDescriptor *Row(uint32_t index)
            {
                return index < rows.size() ? &rows[index] : nullptr;
            }

        public: // ── 写入路径（唯一写者 = collect；生死钩子 = 世界 DeactivateEntityRow）──

            /// 写四元组（**值变才标脏**：静态实体的池 id 是常量 ⇒ 首帧上行一次，
            /// 之后整批零成本；可动实体的 `transform_id` 随帧槽变 ⇒ 自然每帧一行）
            void SetRenderIDs(EntityID id, const graph::RenderItemDescriptor &desc)
            {
                Ensure();

                graph::RenderItemDescriptor *row = Row(id.index);

                if (!row)
                {
                    ReportOverflow(id);
                    return;
                }

                if (*row == desc)
                    return;

                *row = desc;
                MarkDirty(id.index);
            }

            /// 实体销毁：清行（消费者按 `geometry_id == 0` 判无 ⇒ 与行 0 哨兵惯例一致）
            void Clear(EntityID id)
            {
                graph::RenderItemDescriptor *row = Row(id.index);

                if (!row)
                    return;

                const graph::RenderItemDescriptor cleared{};

                if (*row == cleared)
                    return;

                *row = cleared;
                MarkDirty(id.index);
            }

            void ClearAll()
            {
                for (graph::RenderItemDescriptor &row : rows)
                    row = graph::RenderItemDescriptor{};

                if (!row_dirty.empty())
                    std::fill(row_dirty.begin(), row_dirty.end(), 0);
            }

        public: // ── 上传计划（与实体行表同接口形状）──

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
    }
}
