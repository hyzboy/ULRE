#pragma once

#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/EntityGPU.h>
#include<hgl/log/Log.h>
#include<algorithm>
#include<cstdint>
#include<vector>

namespace hgl
{
    namespace ecs
    {
        /**
         * EntityGPUTable —— **实体行表**（`EntityGPU` 行数组）的世界私有存储（T3b）。
         *
         * 设计依据（`doc/future/entity-rows-gpu-cull-plan.md`）：
         *   · **按 `EntityID.index` 直索引**（P-2）—— 实体槽位稳定（T2-a 块池 + generation），
         *     增删实体**不重排**；着色器一次位运算定位，不需要 CPU 维护"紧凑活跃集"。
         *   · **行 0 语义靠 `flags.Active`**：实体销毁只清 `Active`（其余字段不动）⇒ 消费者一律先判 Active。
         *   · `type[i]` / `row[i]` **抄自** `Entity::component_rows[]`（T2-c 的按类型行号表，唯一真源）——
         *     本表是**派生视图**，不新增真值。
         *   · `row[Transform]` = **L2W 行号**（P-1；与 `MeshDrawParams.addr_transform_id` 同源），
         *     由**变换上传路径**给出（`SetTransformRow`），本表不自己推算 —— 动态段的行号含帧槽，
         *     换算的唯一出处是变换存储/系统那一侧。
         *   · 容量 **4096**（设计稿 P-B8-6 拍板，与 Transform static 同口径）；超限 = **fail-fast**
         *     （一次性报错；不扩容、不截断成静默丢实体）。
         *
         * ⚠ 成员声明位置：本表必须**先于 `entity_manager` 声明**（析构逆序 ⇒ 表比实体活得久）：
         *   `~Entity` 会回写本表（清 `Active`），表先死会写到已释放内存。
         */
        class EntityGPUTable
        {
        public:

            /// 容量（设计稿 P-B8-6 冻结值；与 Transform static 段同口径）
            static constexpr uint32_t kCapacity = 4096;

        private:

            std::vector<EntityGPU> rows;        ///< 按 `EntityID.index` 直索引（槽位表）
            std::vector<uint8_t>   row_dirty;   ///< 行脏位（1 B/行；上传后清）
            bool                   overflow_reported = false;

            void MarkDirty(uint32_t index)
            {
                if (row_dirty.size() < rows.size())
                    row_dirty.resize(rows.size(), 0);

                if (index < row_dirty.size())
                    row_dirty[index] = 1;
            }

        private:

            void ReportOverflow(const EntityID id)
            {
                if (overflow_reported)
                    return;

                overflow_reported = true;

                // fail-fast：预算与场景规模不匹配是**项目 bug**（设计稿 P-B8-6：预算制、超限报错、不扩容）
                GLogError("[EntityGPUTable] 实体行表超容量：index=%u ≥ %u ⇒ 该实体不进表"
                          "（容量是拍板冻结值，超限=项目 bug；不扩容、不静默截断）",
                          id.index, kCapacity);
            }

        public:

            bool IsAllocated() const { return !rows.empty(); }

            /// 懒分配：一次 `kCapacity × 128 B = 512 KB`（世界从未建实体则一分不占）
            void Ensure()
            {
                if (rows.empty())
                {
                    rows.resize(kCapacity);
                    row_dirty.assign(kCapacity, 0);
                }
            }

            const EntityGPU *Row(uint32_t index) const
            {
                return index < rows.size() ? &rows[index] : nullptr;
            }

            EntityGPU *Row(uint32_t index)
            {
                return index < rows.size() ? &rows[index] : nullptr;
            }

            const EntityGPU *Row(EntityID id) const { return Row(id.index); }
            EntityGPU       *Row(EntityID id)       { return Row(id.index); }

            static bool IsActive(const EntityGPU &row)
            {
                return (row.flags & uint32_t(EntityGPUFlags::Active)) != 0;
            }

            static bool IsVisible(const EntityGPU &row)
            {
                return (row.flags & uint32_t(EntityGPUFlags::Visible)) != 0;
            }

        public: // ── 写入路径（唯一写者 = 世界；三处：实体生死 / 组件挂卸 / 可见性）──

            /// 实体建立：置 `Active` + `Visible`（`type[]/row[]` 由 `Refresh` 填）。
            /// Visible 默认置位是**与真值同源**的要求：`VisibilityDataStorage` 的默认态
            /// 是可见（`IsEntityVisible` 不在不可见集合 ⇒ true），派生行表若不在建立时
            /// 镜像这个默认值，就出现「真值可见、行表不可见」的漂移（GPU 剔除读行表 ⇒
            /// 全剔空）。之后只有 `SetEntityVisible` 会改它，两条路径闭环。
            void Activate(EntityID id)
            {
                Ensure();

                EntityGPU *row = Row(id);

                if (!row)
                {
                    ReportOverflow(id);
                    return;
                }

                row->flags |= uint32_t(EntityGPUFlags::Active)
                            | uint32_t(EntityGPUFlags::Visible);
                MarkDirty(id.index);
            }

            /// 实体销毁：清 `Active`（其余字段保持原文，供诊断；消费者先判 Active）
            void Deactivate(EntityID id)
            {
                EntityGPU *row = Row(id);

                if (row)
                {
                    row->flags &= ~uint32_t(EntityGPUFlags::Active);
                    MarkDirty(id.index);
                }
            }

            /// 可见性（与 `ECSContext::IsEntityVisible` 同源；唯一真值仍在 VisibilityDataStorage）
            void SetVisible(EntityID id, bool visible)
            {
                EntityGPU *row = Row(id);

                if (!row)
                    return;

                if (visible)
                    row->flags |= uint32_t(EntityGPUFlags::Visible);
                else
                    row->flags &= ~uint32_t(EntityGPUFlags::Visible);

                MarkDirty(id.index);
            }

            /// T3h：阴影接收事实上行（写者=collect 的"本项进批"判据点，与 4-ID 表
            /// 同行；值语义 = CPU FinalizeBatch D3 段已归一的结果 ⇒ 行值可被 gather
            /// 逐字节照抄）。**值变才标脏**——与 SetVisible 不同的地方：每帧都调，
            /// 若照抄无条件 MarkDirty，静态实体会永远脏行（「值变才标脏」纪律）。
            void SetShadowFacts(EntityID id, float bias_multiplier, uint32_t receive)
            {
                EntityGPU *row = Row(id);

                if (!row)
                    return;

                if (row->shadow_bias_multiplier == bias_multiplier
                 && row->shadow_receive == receive)
                    return;

                row->shadow_bias_multiplier = bias_multiplier;
                row->shadow_receive = receive;

                MarkDirty(id.index);
            }

            /// 按实体的**按类型行号表**刷新 `type[]/row[]`（组件挂卸后的唯一同步点）
            void Refresh(const Entity *entity)
            {
                if (!entity)
                    return;

                EntityGPU *row = Row(entity->GetEntityID());

                if (!row)
                    return;

                for (uint32_t t = 1; t < COMPONENT_TYPE_COUNT; ++t)
                {
                    const uint32_t handle = entity->GetComponentRow(static_cast<ComponentType>(t));

                    row->type[t] = handle ? uint8_t(t) : 0;
                    row->row [t] = handle;
                }

                MarkDirty(entity->GetEntityID().index);
            }

            /// `row[Transform]` = **L2W 行号**（P-1）；由变换上传路径下发（0 = 尚无行）
            void SetTransformRow(EntityID id, uint32_t l2w_row)
            {
                EntityGPU *row = Row(id);

                if (!row)
                    return;

                const uint32_t slot_index = uint32_t(ComponentType::Transform);
                const uint8_t  type       = l2w_row ? uint8_t(ComponentType::Transform) : 0;

                // 值变才标脏：L2W 行号对**静态**实体是常量（每次刷新都标脏 = 每帧整批重传，
                // 脏行上传的意义会被吃掉）；可动实体的行号随帧槽变 ⇒ 自然每帧一行。
                if (row->row[slot_index] == l2w_row && row->type[slot_index] == type)
                    return;

                row->row[slot_index]  = l2w_row;
                row->type[slot_index] = type;

                MarkDirty(id.index);
            }

            void Clear()
            {
                for (EntityGPU &row : rows)
                    row = EntityGPU{};

                if (!row_dirty.empty())
                    std::fill(row_dirty.begin(), row_dirty.end(), 0);
            }

        public: // ── 上传计划（T3c-2）：把**脏行**交出去，不整表 512 KB 传 ──

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

        public: // ── 自检（探针 / 测试用）：与实体行号表逐字段对拍 ──

            /// `type[]/row[]` 是否与 `Entity::component_rows[]` 逐字段一致
            /// （`row[Transform]` 不参与：它由变换上传路径给，见 `SetTransformRow`）
            bool VerifyAgainst(const Entity *entity, uint32_t *out_mismatch_type = nullptr,
                               uint32_t *out_mismatch_row = nullptr) const
            {
                if (!entity)
                    return false;

                const EntityGPU *row = Row(entity->GetEntityID());

                if (!row)
                    return false;

                for (uint32_t t = 1; t < COMPONENT_TYPE_COUNT; ++t)
                {
                    if (t == uint32_t(ComponentType::Transform))
                        continue;       // 单独口径（L2W 行），见 SetTransformRow

                    const uint32_t handle = entity->GetComponentRow(static_cast<ComponentType>(t));
                    const uint8_t  type   = handle ? uint8_t(t) : 0;

                    if (row->type[t] != type)
                    {
                        if (out_mismatch_type)
                            *out_mismatch_type = t;

                        return false;
                    }

                    if (row->row[t] != handle)
                    {
                        if (out_mismatch_row)
                            *out_mismatch_row = t;

                        return false;
                    }
                }

                return true;
            }
        };
    }//namespace ecs
}//namespace hgl
