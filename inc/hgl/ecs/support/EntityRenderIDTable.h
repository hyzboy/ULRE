#pragma once

#include <hgl/ecs/support/EntityGPUTable.h>
#include <hgl/ecs/support/RenderPassSlot.h>   // 跨模块单源：pass 槽位/遍数（T4c3a）
#include <hgl/graph/render/RenderItemDescriptor.h>
#include <hgl/ecs/support/DirtyRowSet.h>
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

            // ── T4c3a：按 pass 分半 ────────────────────────────────────────────────
            // 本表行内容是**按 pass 解析**的（阴影 pass 用阴影程序/材质变体，见
            // `GetShadowProgram`）⇒ 一份表存不下两个 pass 的风味：主帧那次写会盖掉阴影的。
            // 分工：每帧槽每 pass 一半，flat 行号 = `PassBase(pass) + entity.index`。
            static constexpr uint32_t kRowStride  = RenderPassSlot::Count * kCapacity;
            static constexpr uint32_t PassBase(uint32_t pass_index)
            { return (pass_index < RenderPassSlot::Count ? pass_index : 0u) * kCapacity; }

        private:

            std::vector<graph::RenderItemDescriptor> rows;   ///< 按 `EntityID.index` 直索引
            /// 行脏位 + 「超容量只报一次」闩（与实体行表 / 光源行表共用 `DirtyRowSet`）
            DirtyRowSet dirty;

            void ReportOverflow(const EntityID id)
            {
                if (!dirty.ArmOverflowWarning())
                    return;

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
                    rows.resize(kRowStride);
                    dirty.Reset(kRowStride);
                }
            }

            /// 按 pass 取行。⚠ 判界必须同时含 `rows.size()`：**未分配**的表（`Ensure()` 尚未调用，
            /// 例如空世界在销毁期走 `Clear()`）上若只判 `index < kCapacity` 会返回悬垂指针 ⇒
            /// 读写越界（实测：测试进程堆损坏 abort）。
            const graph::RenderItemDescriptor *Row(uint32_t pass_index, uint32_t index) const
            {
                const uint32_t flat = PassBase(pass_index) + index;
                return (index < kCapacity && pass_index < RenderPassSlot::Count && flat < rows.size())
                     ? &rows[flat] : nullptr;
            }

            graph::RenderItemDescriptor *Row(uint32_t pass_index, uint32_t index)
            {
                const uint32_t flat = PassBase(pass_index) + index;
                return (index < kCapacity && pass_index < RenderPassSlot::Count && flat < rows.size())
                     ? &rows[flat] : nullptr;
            }

            /// flat 行（= `PassBase(pass) + index`）：**仅供设备上传**逐行拷贝用
            const graph::RenderItemDescriptor *FlatRow(uint32_t flat_index) const
            {
                return flat_index < rows.size() ? &rows[flat_index] : nullptr;
            }

        public: // ── 写入路径（唯一写者 = collect；生死钩子 = 世界 DeactivateEntityRow）──

            /// 写四元组（**值变才标脏**：静态实体的池 id 是常量 ⇒ 首帧上行一次，
            /// 之后整批零成本；可动实体的 `transform_id` 随帧槽变 ⇒ 自然每帧一行）
            void SetRenderIDs(uint32_t pass_index, EntityID id, const graph::RenderItemDescriptor &desc)
            {
                Ensure();

                graph::RenderItemDescriptor *row = Row(pass_index, id.index);

                if (!row)
                {
                    // 越界与越 pass 同口径：都是"进了不该进的地方"= 项目 bug（fail-fast）
                    ReportOverflow(id);
                    return;
                }

                if (*row == desc)
                    return;

                *row = desc;
                dirty.MarkDirty(PassBase(pass_index) + id.index, rows.size());
            }

            /// 实体销毁：清行（消费者按 `geometry_id == 0` 判无 ⇒ 与行 0 哨兵惯例一致）。
            /// T4c3a：**两半都清**——本钩子（`~Entity` → `DeactivateEntityRow`）没有 pass 语境，
            /// 而实体已销毁 ⇒ 两个风味都该消失。
            void Clear(EntityID id)
            {
                const graph::RenderItemDescriptor cleared{};

                for (uint32_t pass_index = 0; pass_index < RenderPassSlot::Count; ++pass_index)
                {
                    graph::RenderItemDescriptor *row = Row(pass_index, id.index);

                    if (!row || *row == cleared)
                        continue;

                    *row = cleared;
                    dirty.MarkDirty(PassBase(pass_index) + id.index, rows.size());
                }
            }

        public: // ── 上传计划（与实体行表同接口形状）──

            /// 脏位集合（`DirtyRowSet`；上传侧直接用它）
            DirtyRowSet       &GetDirtySet()       { return dirty; }
            const DirtyRowSet &GetDirtySet() const { return dirty; }
        };
    }
}
