#pragma once

#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/EntityGPU.h>
#include<hgl/log/Log.h>
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
            bool                   overflow_reported = false;

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
                    rows.resize(kCapacity);
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

            /// 实体建立：置 `Active`（`type[]/row[]` 由 `Refresh` 填）
            void Activate(EntityID id)
            {
                Ensure();

                EntityGPU *row = Row(id);

                if (!row)
                {
                    ReportOverflow(id);
                    return;
                }

                row->flags |= uint32_t(EntityGPUFlags::Active);
            }

            /// 实体销毁：清 `Active`（其余字段保持原文，供诊断；消费者先判 Active）
            void Deactivate(EntityID id)
            {
                EntityGPU *row = Row(id);

                if (row)
                    row->flags &= ~uint32_t(EntityGPUFlags::Active);
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
            }

            /// `row[Transform]` = **L2W 行号**（P-1）；由变换上传路径下发（0 = 尚无行）
            void SetTransformRow(EntityID id, uint32_t l2w_row)
            {
                EntityGPU *row = Row(id);

                if (!row)
                    return;

                row->row[uint32_t(ComponentType::Transform)] = l2w_row;
                row->type[uint32_t(ComponentType::Transform)] = l2w_row ? uint8_t(ComponentType::Transform) : 0;
            }

            void Clear()
            {
                for (EntityGPU &row : rows)
                    row = EntityGPU{};
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
