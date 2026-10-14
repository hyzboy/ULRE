#pragma once

#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/EntityGPU.h>
#include<hgl/ecs/support/DirtyRowSet.h>
#include<hgl/common/RenderOptions.h>   // 编译期开关台帐（§2）：写入侧对拍挂在 ULRE_ECS_DEBUG_API 下
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
         * ── 权威链（四份副本 + 刷新时机；2026-10-13 审计 §2.2 问题 2 选项 (b)）─────────────
         *   ① **权威**：`Entity::component_rows[16]`（1 基行号；唯一写者 `Entity::SetComponentRow`）
         *   ② **本表**（CPU 派生镜像）—— 刷新点：`Activate`（建立：清 `type[]/row[]` 成"无组件"）、
         *      `Refresh`（组件挂卸：全槽）、`SetVisible`（Visible 位）、
         *      **`SetTransformRow`（Transform 槽的\**另一权威** = 变换上传路径，不是 `component_rows[Transform]`）**、
         *      `SetShadowFacts`（阴影事实上行；写者 = collect 的进批判据点）
         *   ③ GPU SSBO `EntityGPU` 行（**按世界地址槽切份**；只传脏行，写者 = 上传播径）
         *   ④ GLSL 只读镜像（`ShaderLibrary/ecs/entity_gpu_cull.glsl`；实际只读 `flags` 与 `row[Transform]`）
         *   ⇒ 本表是**派生视图**，不新增真值；`row[Transform]` 是链上唯一的"两个权威"点。
         *   ⚠ `EntityGPU::type[16]` 是**有意冗余**（槽号已蕴含类型；保 shader 判空写法与 `offsetof` 断言）——**别当冗余删**。
         *   守门：`VerifyAfterWrite()`（debug 面，开关 = 台帐 `ULRE_ECS_DEBUG_API`）在①三个写入点核 ②↔① 的 15 槽。
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
            /// 行脏位 + 「超容量只报一次」闩：与 `EntityRenderIDTable` / `LightRowTable` 共用
            /// `DirtyRowSet`（此前三张表各复制一份逐字相同的机制）
            DirtyRowSet            dirty;

        private:

            void ReportOverflow(const EntityID id)
            {
                if (!dirty.ArmOverflowWarning())
                    return;

                // fail-fast：预算与场景规模不匹配是**项目 bug**（设计稿 P-B8-6：预算制、超限报错、不扩容）
                GLogError("[EntityGPUTable] 实体行表超容量：index=%u ≥ %u ⇒ 该实体不进表"
                          "（容量是拍板冻结值，超限=项目 bug；不扩容、不静默截断）",
                          id.index, kCapacity);
            }

        public:

            /// 懒分配：一次 `kCapacity × 128 B = 512 KB`（世界从未建实体则一分不占）
            void Ensure()
            {
                if (rows.empty())
                {
                    rows.resize(kCapacity);
                    dirty.Reset(kCapacity);
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
            /// ⚠ 镜像是权威的**扁平默认**（=「没有被显式设过不可见」），**不是**「此刻对权威可见」：
            /// 父已不可见之后才建立的后代，本位会与含继承的权威短时不一致（已知失步，见
            /// doc/visibility-authority-chain.md §3），由 4-ID 表 / 批标签两道闸门兜住。
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

                // 槽位复用（T2-a 块池 + generation）：本行可能还留着**上一个占用者**的
                // `type[]/row[]`（`Deactivate` 按诊断口径刻意不清）。新实体在挂第一个组件之前
                // 与 `component_rows[]`（全新实体 = 全 0）不一致 ⇒ 会让下面 §2.2 问题 2 的
                // 写入侧对拍**假红**。这里清成"无组件"（与权威同源；挂卸仍由 `Refresh` 全槽同步）。
                for (uint32_t t = 1; t < COMPONENT_TYPE_COUNT; ++t)
                {
                    row->type[t] = 0;
                    row->row [t] = 0;
                }

                dirty.MarkDirty(id.index, rows.size());
            }

            /// 实体销毁：清 `Active`（其余字段保持原文，供诊断；消费者先判 Active）
            void Deactivate(EntityID id)
            {
                EntityGPU *row = Row(id);

                if (row)
                {
                    row->flags &= ~uint32_t(EntityGPUFlags::Active);
                    dirty.MarkDirty(id.index, rows.size());
                }
            }

            /// 可见性：写**扁平派生位**（不继承祖先）。
            /// ⚠ 权威 = `VisibilityDataStorage` 的缺席集（**负向 + 含祖先继承**）；本位只镜像
            /// `SetEntityVisible` 落在该实体自身上的值 ⇒ 父不可见时后代本位仍可能为 1，
            /// 消费者**不得仅凭它判定「该画」**（闸门 = 4-ID 表 + 批标签）。
            /// 本表**不得**回查可见性存储、也不得上溯祖先（那等于在世界里再造一份继承语义）。
            /// 权威链与已知失步见 doc/visibility-authority-chain.md。
            void SetVisible(EntityID id, bool visible)
            {
                EntityGPU *row = Row(id);

                if (!row)
                    return;

                if (visible)
                    row->flags |= uint32_t(EntityGPUFlags::Visible);
                else
                    row->flags &= ~uint32_t(EntityGPUFlags::Visible);

                dirty.MarkDirty(id.index, rows.size());
            }

            /// T3h：阴影接收事实上行（写者=collect 的"本项进批"判据点，与 4-ID 表
            /// 同行；值语义 = 实体级自由函数 + collect 归一（原 CPU 行写入段已删）⇒ 行值可被 gather
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

                dirty.MarkDirty(id.index, rows.size());
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

                dirty.MarkDirty(entity->GetEntityID().index, rows.size());
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

                dirty.MarkDirty(id.index, rows.size());
            }

        public: // ── 上传计划（T3c-2）：把**脏行**交出去，不整表 512 KB 传 ──

            /// 脏位集合（`DirtyRowSet`；上传侧直接用它：GetDirtyCount / CollectDirtyRows / ClearDirty）
            DirtyRowSet       &GetDirtySet()       { return dirty; }
            const DirtyRowSet &GetDirtySet() const { return dirty; }

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

            /// **写入侧对拍**（2026-10-13；审计 §2.2 问题 2 选项 (a)）：在**三个刷新点**——
            /// 组件挂卸（`Refresh` 的调用侧）、可见性（`SetVisible`）、变换行（`SetTransformRow`）——
            /// 调用，把"写入者恰好覆盖全部槽"从口头约束变成 debug 期可验证的断言。
            ///
            /// **生产零开销**：开关 = 台帐 `hgl/common/RenderOptions.h` §2 的 `ULRE_ECS_DEBUG_API`
            /// （默认 AUTO：Debug 开 / Release 关）⇒ 关掉时本函数体为空 inline。
            /// `row[Transform]` 不参与比对（单独口径，见 `SetTransformRow`）；空实体、超容量未进表
            /// 都**不算失步**（后者 `Activate` 已报过一次）。
            void VerifyAfterWrite(const Entity *entity, const char *what)
            {
            #if ULRE_ECS_DEBUG_API
                if (!entity)
                    return;

                const EntityGPU *row = Row(entity->GetEntityID());

                if (!row)
                    return;

                uint32_t bad_type = 0;
                uint32_t bad_row  = 0;

                if (VerifyAgainst(entity, &bad_type, &bad_row))
                    return;

                const uint32_t slot = bad_row ? bad_row : bad_type;

                GLogError(u8"[EntityGPUTable] 实体行表与 component_rows 失步（写入点=%s）："
                          u8"entity=%s id=(%u,%u) mismatch_type=%u mismatch_row=%u；"
                          u8"该槽 type[%u]=%u row[%u]=%u，权威 component_rows[%u]=%u",
                          what, entity->GetName().c_str(),
                          entity->GetEntityID().index, entity->GetEntityID().generation,
                          bad_type, bad_row,
                          slot, row->type[slot], slot, row->row[slot],
                          slot, entity->GetComponentRow(static_cast<ComponentType>(slot)));
            #else
                (void)entity;
                (void)what;
            #endif//ULRE_ECS_DEBUG_API
            }

            /// **可见性镜像对拍**（2026-10-14；审计 §2.2 问题 6 / ECS #4 选项 (c) 的**观察面**）：
            /// 比较**权威**（`ECSContext::IsEntityVisible` = 负向缺席集 + **含祖先继承**）与**派生位 ②**
            /// （`EntityGPUFlags::Visible` = **扁平**，不继承）。
            ///
            /// ⚠ 本对拍报的是**已知失步**（父不可见不向后代传播 / 父不可见之后才建立 / 重挂父链
            ///   认了不可见父），**不是缺陷**：按构造永不改画面（闸门 = 4-ID 表 `geometry_id == 0`
            ///   跳过 + 批标签 `0xFFFFFFFF` 跳过）⇒ 因此**有界点名 + 计数**，不做 fail-fast 门。
            ///   口径与成因见 `doc/visibility-authority-chain.md` §3。
            ///
            /// **生产零开销**：开关 = 台帐 `hgl/common/RenderOptions.h` §2 的 `ULRE_ECS_DEBUG_API`
            /// （默认 AUTO：Debug 开 / Release 关）⇒ 关掉时函数体为空 inline，且**调用点也不求值**
            ///   （门加在触发点，见 `RenderPrimitiveCollectSystem` 的接线处）。
            /// 取数：`GetVisibleMirrorCount()`（累计**失步条目**数；探针前后取差 = 本段/本帧条数）。
            void VerifyVisibleMirror(const Entity *entity, bool authority_visible, const char *what)
            {
            #if ULRE_ECS_DEBUG_API
                if (!entity)
                    return;

                const uint32_t index = entity->GetEntityID().index;

                if (index >= rows.size())
                    return;

                const bool mirror = (rows[index].flags & uint32_t(EntityGPUFlags::Visible)) != 0;

                if (mirror == authority_visible)
                    return;

                ++visible_mirror_count;

                if (visible_mirror_warned.size() < rows.size())
                    visible_mirror_warned.resize(rows.size(), 0);

                if (visible_mirror_warned[index])            // 每个实体行只点名一次（不刷屏）
                    return;

                visible_mirror_warned[index] = 1;

                GLogWarning(u8"[EntityGPUTable] 可见性镜像与权威不同源（%s）：entity=%s id=(%u,%u) "
                            u8"权威=%u 派生位=%u；已知失步（父不可见不传播 / 后建立 / 重挂父链），"
                            u8"按构造不会错画（闸门 = 4-ID 表 geometry_id==0 + 批标签 0xFFFFFFFF）；"
                            u8"成因与口径见 doc/visibility-authority-chain.md §3。本报告只点名一次/行。",
                            what, entity->GetName().c_str(),
                            index, entity->GetEntityID().generation,
                            authority_visible ? 1u : 0u, mirror ? 1u : 0u);
            #else
                (void)entity;
                (void)authority_visible;
                (void)what;
            #endif//ULRE_ECS_DEBUG_API
            }

            /// 累计**失步条目**数（对拍器取数口；仅在 `ULRE_ECS_DEBUG_API` 下累加）
            uint32_t GetVisibleMirrorCount() const
            {
            #if ULRE_ECS_DEBUG_API
                return visible_mirror_count;
            #else
                return 0;
            #endif//ULRE_ECS_DEBUG_API
            }

        private: // ── 可见性镜像对拍的状态（**常驻**：与配置无关，避免 `ECSContext` 布局分叉）──

            /// ⚠ 刻意**不按宏门住**：全仓无「门住成员」先例，而本表是 `ECSContext` 的**值成员** ⇒ 门住会让
            /// Debug / Release 的 `sizeof(ECSContext)` 不同（陈旧 TU 的错位只在 Debug 暴露，更难查）。
            /// 常驻成本 = vector 头 + 4 B；闩向量**只在 Debug 首次失步时**惰性分配（Release 零分配）。
            std::vector<uint8_t> visible_mirror_warned;   ///< 每实体行只点名一次的闩（debug 面用）
            uint32_t             visible_mirror_count = 0;///< 累计失步条目数（debug 面用）
        };
    }//namespace ecs
}//namespace hgl
