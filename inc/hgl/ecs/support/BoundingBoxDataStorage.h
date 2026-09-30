#pragma once

#include<glm/glm.hpp>
#include<cstdint>
#include<array>
#include<hgl/type/ValueArray.h>
#include<hgl/type/UnorderedMap.h>
#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/math/geometry/AABB.h>

namespace hgl
{
    namespace ecs
    {
        /**
         * BoundingBoxDataStorage —— 连续 SoA 存储 + 行世代句柄（与 TransformDataStorage 同范式）。
         *
         * 行字段全部是并行 `hgl::ValueArray`（逐行连续，可直接作为未来 GPU Storage Buffer 的候选）：
         *   local_min / local_max（**真源**）· world_min / world_max（派生缓存）· world_valid · generations · owners，
         * 另有 `entity_rows`（EntityID → 行）反向索引。
         *
         * **为什么行里只存 min/max、不直接存 `math::AABB`**：`math::AABB` 自带 352 B 的派生缓存
         * （4 条 vec3 本体之外的 `face_center_point[6]` + `planes[6]`，专供面/平面求交），
         * 而行存储只需要 min/max；存 AABB 会让每行从 ~77 B 膨胀到 717 B（≈9.3×），
         * 且那 352 B 派生缓存绝大多数行存了永远不读。派生量（center/extent/面/平面）按需算即可。
         * ⇒ **不要「优化」回 `ValueArray<math::AABB>`**（会连带需要给 math::AABB 打全局 operator== 补丁）。
         *
         * 与旧实现（std::vector + freeList + dirtyFlags + centers/extents 缓存）的差异：
         *   · **freeList 删除**：行不复用、释放即失效（Deallocate 把世代归 0）。
         *     将来若引入行复用，必须在既有世代上 **+2 保持奇数**（ABA 免疫）。
         *   · **dirtyFlags / IsDirty / ClearDirty 删除**：全仓零消费者。
         *   · **centers / extents 缓存数组删除**：可由 min/max 派生，
         *     `GetCenter/GetExtents` 直接算 `(min+max)*0.5` / `(max-min)*0.5`。
         *
         * 世代约定（与 Transform 完全一致）：**0 = 死/未分配；正奇数 = 活**。
         */
        class BoundingBoxDataStorage
        {
        public:

            using HandleID = uint32_t;
            static constexpr HandleID INVALID_HANDLE = UINT32_MAX;

        private:

            // ── 核心行数据（对齐未来 GPU Storage Buffer）──
            // 每行 = min/max 两对 vec3（每列 16 B）；**不存 math::AABB**——它带 352 B 派生缓存，见类注释
            hgl::ValueArray<glm::vec3>      local_min;          // 本地空间 min（真源）
            hgl::ValueArray<glm::vec3>      local_max;          // 本地空间 max（真源）
            hgl::ValueArray<glm::vec3>      world_min;          // 世界空间 min（派生：本地 AABB × 世界矩阵）
            hgl::ValueArray<glm::vec3>      world_max;          // 世界空间 max（派生）
            hgl::ValueArray<uint8_t>        world_valid;        // 1 byte each（world_bounds 是否已算出）

            // ── 行元数据 ──
            hgl::ValueArray<EntityID>       owners;             // 所属实体（accessor 解析 owner）
            hgl::ValueArray<uint32_t>       generations;        // 行世代：释放时归 0 ⇒ 旧句柄读时失效

            /// 实体 → AABB 行（反向索引）；"某实体有没有包围盒"不再靠组件查询。
            hgl::UnorderedMap<EntityID,HandleID> entity_rows;

        public:

            /// 分配一个新行（无参：owner 另行 SetOwner）。**行不复用**：每次取尾部新行。
            HandleID Allocate()
            {
                const HandleID id = static_cast<HandleID>(local_min.GetCount());

                // 默认盒 = `math::AABB()` 构造结果：min=(0,0,0) / max=(1,1,1)
                local_min.Add(glm::vec3(0.0f));
                local_max.Add(glm::vec3(1.0f));
                world_min.Add(glm::vec3(0.0f));
                world_max.Add(glm::vec3(1.0f));
                world_valid.Add(0);

                owners.Add(EntityID());
                generations.Add(1);         // 新行从 1 起（正奇数 = 活；0 = 死/未分配）

                return id;
            }

            /// 释放一行
            void Deallocate(HandleID id)
            {
                if (id >= static_cast<HandleID>(local_min.GetCount()))
                    return;

                // 失效机制：释放即换代 ⇒ 任何持有旧世代的行句柄立刻作废。
                // 世代编码约定：**0 = 死/未分配；正奇数 = 活**。
                // 将来若引入行复用时：复用必须在既有世代上 **+2 保持奇数**（ABA 免疫，直到 2^31 次复用）。
                generations[id] = 0;

                world_valid[id] = 0;

                const EntityID owner = owners[id];

                if (owner.IsValid())
                {
                    const HandleID *row = entity_rows.GetValuePointer(owner);

                    if (row && *row == id)
                        entity_rows.DeleteByKey(owner);
                }

                owners[id] = EntityID();
            }

            /// 行数（含已释放的死行：行不复用 ⇒ 行索引稳定）
            uint32_t GetCount() const
            {
                return static_cast<uint32_t>(local_min.GetCount());
            }

            /// 行世代（句柄失效检测）。越界或从未分配返回 0 —— 默认构造的句柄世代也是 0 ⇒ 不会误判为有效。
            uint32_t GetGeneration(HandleID id) const
            {
                if (id >= static_cast<HandleID>(generations.GetCount()))
                    return 0;

                return generations[id];
            }

            /// 该行是否已分配（世代非 0）
            bool IsAllocated(HandleID id) const
            {
                return GetGeneration(id) != 0;
            }

        public: // 元数据（owner）—— 行归属的唯一真源

            EntityID GetOwner(HandleID id) const
            {
                if (id >= static_cast<HandleID>(owners.GetCount()))
                    return EntityID();

                return owners[id];
            }

            void SetOwner(HandleID id,EntityID owner)
            {
                if (id >= static_cast<HandleID>(owners.GetCount()))
                    return;

                const EntityID old = owners[id];

                if (old.IsValid() && old != owner)
                {
                    const HandleID *row = entity_rows.GetValuePointer(old);

                    if (row && *row == id)
                        entity_rows.DeleteByKey(old);
                }

                owners[id] = owner;

                if (owner.IsValid())
                    entity_rows[owner] = id;
            }

            /// 实体 → AABB 行（无效/无行时返回 INVALID_HANDLE）
            HandleID FindByOwner(EntityID owner) const
            {
                if (!owner.IsValid())
                    return INVALID_HANDLE;

                const HandleID *row = entity_rows.GetValuePointer(owner);

                return row ? *row : INVALID_HANDLE;
            }

        public: // 本地包围盒（真源：拆成 min/max 两列存，读时按需重组 AABB）

            void SetLocalBounds(HandleID id,const math::AABB &bounds)
            {
                if (id >= static_cast<HandleID>(local_min.GetCount()))
                    return;

                local_min[id] = glm::vec3(bounds.GetMin());
                local_max[id] = glm::vec3(bounds.GetMax());
            }

            /// 按需由 min/max 重组 `math::AABB`（存储只存 min/max，AABB 的 352 B 派生缓存随用随建）
            math::AABB GetLocalBounds(HandleID id) const
            {
                math::AABB bounds;

                if (id >= static_cast<HandleID>(local_min.GetCount()))
                    return bounds;

                bounds.SetMinMax(glm::vec3(local_min[id]),glm::vec3(local_max[id]));

                return bounds;
            }

            /// 中心 = (min + max) * 0.5（派生，不单独缓存）
            glm::vec3 GetCenter(HandleID id) const
            {
                if (id >= static_cast<HandleID>(local_min.GetCount()))
                    return glm::vec3(0.0f);

                return glm::vec3((local_min[id] + local_max[id]) * 0.5f);
            }

            /// 半长 = (max - min) * 0.5（派生，不单独缓存）
            glm::vec3 GetExtents(HandleID id) const
            {
                if (id >= static_cast<HandleID>(local_min.GetCount()))
                    return glm::vec3(0.0f);

                return glm::vec3((local_max[id] - local_min[id]) * 0.5f);
            }

        public: // 世界包围盒（派生缓存：本地 AABB × 世界矩阵；同样拆 min/max 存）

            void SetWorldBounds(HandleID id,const math::AABB &bounds)
            {
                if (id >= static_cast<HandleID>(world_min.GetCount()))
                    return;

                world_min[id]   = glm::vec3(bounds.GetMin());
                world_max[id]   = glm::vec3(bounds.GetMax());
                world_valid[id] = 1;
            }

            bool HasWorldBounds(HandleID id) const
            {
                if (!IsAllocated(id))
                    return false;

                return id < static_cast<HandleID>(world_valid.GetCount()) && world_valid[id] != 0;
            }

            /// 按需由 min/max 重组 `math::AABB`（同 GetLocalBounds）
            math::AABB GetWorldBounds(HandleID id) const
            {
                math::AABB bounds;

                if (id >= static_cast<HandleID>(world_min.GetCount()))
                    return bounds;

                bounds.SetMinMax(glm::vec3(world_min[id]),glm::vec3(world_max[id]));

                return bounds;
            }

        public: // 每行字节账目（探针 / 测试共用；**新增每行数组时只改这一处**）

            /// 一条平行数组的账目
            struct PerRowField
            {
                const char *name;
                uint32_t    bytes;   ///< 该数组每行占的字节（= 元素大小）
                uint32_t    count;   ///< 元素数（不变量：恒等于 GetCount()）
            };

            static constexpr uint32_t PER_ROW_FIELD_COUNT = 7;

            using PerRowFieldTable = std::array<PerRowField,PER_ROW_FIELD_COUNT>;

            /// 逐条列出每行数组。**新增/删除每行数组时同步改这里**。
            /// 不含 `entity_rows`（哈希表，堆开销由分配统计覆盖）。
            PerRowFieldTable GetPerRowFields() const
            {
                return { {
                    { "local_min",    static_cast<uint32_t>(sizeof(glm::vec3)), static_cast<uint32_t>(local_min.GetCount()) },
                    { "local_max",    static_cast<uint32_t>(sizeof(glm::vec3)), static_cast<uint32_t>(local_max.GetCount()) },
                    { "world_min",    static_cast<uint32_t>(sizeof(glm::vec3)), static_cast<uint32_t>(world_min.GetCount()) },
                    { "world_max",    static_cast<uint32_t>(sizeof(glm::vec3)), static_cast<uint32_t>(world_max.GetCount()) },
                    { "world_valid",  static_cast<uint32_t>(sizeof(uint8_t)),   static_cast<uint32_t>(world_valid.GetCount()) },
                    { "owner",        static_cast<uint32_t>(sizeof(EntityID)),  static_cast<uint32_t>(owners.GetCount()) },
                    { "generation",   static_cast<uint32_t>(sizeof(uint32_t)),  static_cast<uint32_t>(generations.GetCount()) },
                } };
            }

            /// 每行字节合计（各平行数组元素大小之和）
            uint32_t PerRowBytes() const
            {
                uint32_t total = 0;

                for (const PerRowField &f : GetPerRowFields())
                    total += f.bytes;

                return total;
            }

            /// 不变量检查：每个每行数组的元素数都必须等于行数。
            /// 返回第一个不匹配的字段名（全一致返回 nullptr）——漏在 Allocate/Deallocate 里同步的数组会在这里现形。
            const char *FindPerRowCountMismatch() const
            {
                const uint32_t rows = GetCount();

                for (const PerRowField &f : GetPerRowFields())
                    if (f.count != rows)
                        return f.name;

                return nullptr;
            }

        public:

            size_t GetSize() const { return static_cast<size_t>(local_min.GetCount()); }

            void Clear()
            {
                local_min.Clear();
                local_max.Clear();
                world_min.Clear();
                world_max.Clear();
                world_valid.Clear();
                owners.Clear();
                generations.Clear();
                entity_rows.Clear();
            }
        };
    }//namespace ecs
}//namespace hgl
