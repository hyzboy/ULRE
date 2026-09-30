#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <hgl/type/ValueArray.h>
#include <hgl/ecs/core/EntityHandle.h>
#include <functional>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace hgl
{
    namespace ecs
    {
        /**
         * Optimized SOA (Structure of Arrays) storage for transform data
         * Provides flat triplet layout (local_matrices, parent_indices, world_matrices)
         * with level-by-level topological order for object-independent evaluation
         * (ready for future ComputeShader migration).
         */
        class TransformDataStorage
        {
        public:

            using HandleID = uint32_t;
            static constexpr HandleID INVALID_HANDLE = UINT32_MAX;

        private:

            // ── 核心三元组数据（对应未来 GPU Storage Buffer）──
            hgl::ValueArray<glm::mat4>      local_matrices;     // 64 bytes each, consecutive
            hgl::ValueArray<HandleID>       parent_indices;     // 4 bytes each, consecutive (index in same storage)
            hgl::ValueArray<glm::mat4>      world_matrices;     // 64 bytes each, consecutive (final L2W)

            // ── 层级拓扑分层（Level-by-Level）调度 ──
            hgl::ValueArray<uint16_t>       hierarchy_depths;   // 2 bytes each (0 = root, 1, 2...)
            hgl::ValueArray<HandleID>       eval_order;         // 拓扑计算序列 (Level 0 先算, Level 1 其次...)
            hgl::ValueArray<uint32_t>       level_offsets;      // 各 Level 在 eval_order 中的起始偏移
            bool                            topology_dirty = true;

            // ── 本地原始属性（用于快速修改与交互）──
            hgl::ValueArray<glm::vec3>      positions;          // 12 bytes each, consecutive
            hgl::ValueArray<glm::quat>      rotations;          // 16 bytes each, consecutive
            hgl::ValueArray<glm::vec3>      scales;             // 12 bytes each, consecutive

            // ── 状态标记 ──
            hgl::ValueArray<uint8_t>        local_dirty;        // 1 byte each (本地 TRS 矩阵是否需要重新合成)
            hgl::ValueArray<uint8_t>        matrixDirty;        // 1 byte each (世界矩阵是否脏)
            hgl::ValueArray<uint8_t>        mobility;           // 1 byte each (0=static, 1=movable)

            /// 子节点表（与行一一对应；CPU 侧层级查询用）
            std::vector<std::vector<HandleID>> children;

            // ── 变换元数据（T8：原先在 TransformComponent 里，现搬进存储 ⇒ 单一真源）──
            hgl::ValueArray<EntityID>       owners;             // 所属实体（accessor 解析 owner / 告警里的实体名）
            hgl::ValueArray<uint32_t>       change_masks;       // 变更位掩码（TouchChange 累积）
            hgl::ValueArray<uint64_t>       versions;           // 变更计数（渲染侧比对"是否已上传"）
            hgl::ValueArray<uint8_t>        write_armed;        // D4：该行已被渲染侧消费过
            hgl::ValueArray<uint8_t>        write_warned;       // D4：已就"运行期写静态"告警过一次

            /// 实体 → 变换行（反向索引）。T8 起"某实体有没有变换"不再靠组件查询。
            std::unordered_map<EntityID,HandleID> entity_rows;

        public:

            /// Allocate space for a new transform
            HandleID Allocate()
            {
                HandleID id = static_cast<HandleID>(positions.GetCount());
                positions.Add(glm::vec3(0.0f));
                rotations.Add(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                scales.Add(glm::vec3(1.0f));

                local_matrices.Add(glm::mat4(1.0f));
                parent_indices.Add(INVALID_HANDLE);
                world_matrices.Add(glm::mat4(1.0f));

                hierarchy_depths.Add(0);
                local_dirty.Add(1);
                matrixDirty.Add(1);
                mobility.Add(1);  // Default to movable

                owners.Add(EntityID());
                change_masks.Add(0);
                versions.Add(0);
                write_armed.Add(0);
                write_warned.Add(0);
                children.emplace_back();

                topology_dirty = true;
                return id;
            }

            /// Free allocated space
            void Deallocate(HandleID id)
            {
                if (id >= static_cast<HandleID>(positions.GetCount()))
                    return;

                // Mark as unused but keep slot to preserve stable indices
                parent_indices[id] = INVALID_HANDLE;
                matrixDirty[id] = 0;
                local_dirty[id] = 0;

                if (id < static_cast<HandleID>(owners.GetCount()))
                {
                    const EntityID owner = owners[id];

                    if (owner.IsValid())
                    {
                        const auto it = entity_rows.find(owner);

                        if (it != entity_rows.end() && it->second == id)
                            entity_rows.erase(it);
                    }

                    owners[id] = EntityID();
                }

                if (id < static_cast<HandleID>(children.size()))
                    children[id].clear();

                topology_dirty = true;
            }

        public: // Position accessors - SOA optimized

            glm::vec3 GetPosition(HandleID id) const
            {
                return positions[id];
            }

            void SetPosition(HandleID id, const glm::vec3& pos)
            {
                positions[id] = pos;
                local_dirty[id] = 1;
                matrixDirty[id] = 1;
            }

        public: // Rotation accessors - SOA optimized

            glm::quat GetRotation(HandleID id) const
            {
                return rotations[id];
            }

            void SetRotation(HandleID id, const glm::quat& rot)
            {
                rotations[id] = rot;
                local_dirty[id] = 1;
                matrixDirty[id] = 1;
            }

        public: // Scale accessors - SOA optimized

            glm::vec3 GetScale(HandleID id) const
            {
                return scales[id];
            }

            void SetScale(HandleID id, const glm::vec3& scale)
            {
                scales[id] = scale;
                local_dirty[id] = 1;
                matrixDirty[id] = 1;
            }

            void SetLocalTRS(HandleID id, const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale)
            {
                positions[id] = pos;
                rotations[id] = rot;
                scales[id] = scale;
                local_dirty[id] = 1;
                matrixDirty[id] = 1;
            }

        public: // Local matrix accessors
                 // local_matrices 是**求值中间量**（由 TRS 合成，供两个平铺世界求值读），
                 // 不是可写的第二真源：写入只有 TRS 与求值两条路。

            void UpdateLocalMatrix(HandleID id)
            {
                if (id >= static_cast<HandleID>(positions.GetCount()))
                    return;

                glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scales[id]);
                glm::mat4 rotMatrix = glm::mat4_cast(rotations[id]);
                glm::mat4 transMatrix = glm::translate(glm::mat4(1.0f), positions[id]);

                local_matrices[id] = transMatrix * rotMatrix * scaleMatrix;
                local_dirty[id] = 0;
            }

        public: // World matrix accessors

            glm::mat4 GetWorldMatrix(HandleID id) const
            {
                return world_matrices[id];
            }

            void SetWorldMatrix(HandleID id, const glm::mat4& matrix)
            {
                world_matrices[id] = matrix;
                matrixDirty[id] = 0;
            }

        public: // 元数据（owner / 变更版本 / D4 标记）—— T8 起是唯一真源

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
                    const auto it = entity_rows.find(old);

                    if (it != entity_rows.end() && it->second == id)
                        entity_rows.erase(it);
                }

                owners[id] = owner;

                if (owner.IsValid())
                    entity_rows[owner] = id;
            }

            /// 实体 → 变换行（无效/无变换时返回 INVALID_HANDLE）
            HandleID FindByOwner(EntityID owner) const
            {
                if (!owner.IsValid())
                    return INVALID_HANDLE;

                const auto it = entity_rows.find(owner);

                return (it != entity_rows.end()) ? it->second : INVALID_HANDLE;
            }

            uint32_t GetChangeMask(HandleID id) const
            {
                if (id >= static_cast<HandleID>(change_masks.GetCount()))
                    return 0;

                return change_masks[id];
            }

            void ClearChangeMask(HandleID id)
            {
                if (id < static_cast<HandleID>(change_masks.GetCount()))
                    change_masks[id] = 0;
            }

            /// 记一次变更：版本 +1 且累积位掩码（与 Component::TouchChange 同语义）
            void TouchChange(HandleID id,uint32_t mask)
            {
                if (id >= static_cast<HandleID>(versions.GetCount()))
                    return;

                ++versions[id];
                change_masks[id] |= mask;
            }

            void AddChangeMask(HandleID id,uint32_t mask)
            {
                if (id < static_cast<HandleID>(change_masks.GetCount()))
                    change_masks[id] |= mask;
            }

            uint64_t GetVersion(HandleID id) const
            {
                if (id >= static_cast<HandleID>(versions.GetCount()))
                    return 0;

                return versions[id];
            }

            bool IsWriteArmed(HandleID id) const
            {
                return (id < static_cast<HandleID>(write_armed.GetCount())) && write_armed[id] != 0;
            }

            void ArmWrite(HandleID id)
            {
                if (id < static_cast<HandleID>(write_armed.GetCount()))
                    write_armed[id] = 1;
            }

            bool HasWarnedWrite(HandleID id) const
            {
                return (id < static_cast<HandleID>(write_warned.GetCount())) && write_warned[id] != 0;
            }

            void SetWriteWarned(HandleID id)
            {
                if (id < static_cast<HandleID>(write_warned.GetCount()))
                    write_warned[id] = 1;
            }

        public: // 子节点（CPU 侧层级查询；GPU 侧走 parent_indices + eval_order，与此无关）

            const std::vector<HandleID> &GetChildren(HandleID id) const
            {
                static const std::vector<HandleID> kEmpty;

                if (id >= static_cast<HandleID>(children.size()))
                    return kEmpty;

                return children[id];
            }

            void AddChild(HandleID id,HandleID child)
            {
                if (id >= static_cast<HandleID>(children.size()) || child == id)
                    return;

                auto &list = children[id];

                for (const HandleID c : list)
                    if (c == child)
                        return;

                list.push_back(child);
            }

            void RemoveChild(HandleID id,HandleID child)
            {
                if (id >= static_cast<HandleID>(children.size()))
                    return;

                auto &list = children[id];

                for (auto it = list.begin(); it != list.end(); ++it)
                {
                    if (*it == child)
                    {
                        list.erase(it);
                        return;
                    }
                }
            }

        public: // Parent relationship

            HandleID GetParent(HandleID id) const
            {
                return parent_indices[id];
            }

            void SetParent(HandleID id, HandleID parentId)
            {
                if (parent_indices[id] != parentId)
                {
                    parent_indices[id] = parentId;
                    matrixDirty[id] = 1;
                    topology_dirty = true;
                }
            }

        public: // Dirty flag

            bool IsDirty(HandleID id) const
            {
                return matrixDirty[id] != 0;
            }

            void SetDirty(HandleID id, bool dirty)
            {
                matrixDirty[id] = dirty ? 1 : 0;
            }

        public: // Mobility tracking (0 = static, 1 = movable)

            uint8_t GetMobility(HandleID id) const
            {
                return mobility[id];
            }

            void SetMobility(HandleID id, uint8_t mobilityValue)
            {
                mobility[id] = mobilityValue;
            }

        public: // Flat arrays accessors (for GPU uploads / ComputeShader dispatch)

            const glm::mat4* GetLocalMatricesData() const { return local_matrices.GetData(); }
            const HandleID* GetParentIndicesData() const { return parent_indices.GetData(); }
            const glm::mat4* GetWorldMatricesData() const { return world_matrices.GetData(); }
            glm::mat4* GetWorldMatricesData() { return world_matrices.GetData(); }

            const HandleID* GetEvalOrderData() const { return eval_order.GetData(); }

            void UpdateAllLocalMatrices()
            {
                const int count = local_dirty.GetCount();
                for (int i = 0; i < count; ++i)
                {
                    if (local_dirty[i])
                        UpdateLocalMatrix(i);
                }
            }

        public: // 拓扑（供示例/probe 读层级；重建由脏标记驱动）

            bool IsTopologyDirty() const { return topology_dirty; }

            /// 重建树深度分层与拓扑执行序列
            void RebuildTopologyOrder()
            {
                const int count = positions.GetCount();
                if (count <= 0)
                {
                    eval_order.Clear();
                    level_offsets.Clear();
                    topology_dirty = false;
                    return;
                }

                hierarchy_depths.Resize(count);
                uint16_t max_depth = 0;

                hgl::ValueArray<uint8_t> visited;
                visited.Resize(count);
                std::memset(visited.GetData(), 0, count * sizeof(uint8_t));

                for (int i = 0; i < count; ++i)
                {
                    if (visited[i] == 2)
                        continue;

                    int curr = i;
                    int depth = 0;
                    while (curr >= 0 && curr < count && depth < 256)
                    {
                        visited[curr] = 1;
                        HandleID p = parent_indices[curr];
                        if (p == INVALID_HANDLE || p >= static_cast<HandleID>(count) || p == static_cast<HandleID>(curr))
                            break;

                        if (visited[p] == 2)
                        {
                            depth += hierarchy_depths[p] + 1;
                            break;
                        }
                        if (visited[p] == 1) // 环路保护
                        {
                            parent_indices[curr] = INVALID_HANDLE;
                            break;
                        }
                        curr = static_cast<int>(p);
                        ++depth;
                    }

                    curr = i;
                    int cur_d = depth;
                    while (curr >= 0 && curr < count && visited[curr] == 1)
                    {
                        visited[curr] = 2;
                        hierarchy_depths[curr] = static_cast<uint16_t>(cur_d);
                        if (cur_d > max_depth)
                            max_depth = static_cast<uint16_t>(cur_d);
                        HandleID p = parent_indices[curr];
                        if (p == INVALID_HANDLE || p >= static_cast<HandleID>(count))
                            break;
                        curr = static_cast<int>(p);
                        --cur_d;
                    }
                }

                // 2. 按 Level 桶排序生成 eval_order 与 level_offsets
                eval_order.Resize(count);
                level_offsets.Clear();

                int current_out = 0;
                for (uint16_t level = 0; level <= max_depth; ++level)
                {
                    level_offsets.Add(static_cast<uint32_t>(current_out));
                    for (int i = 0; i < count; ++i)
                    {
                        if (hierarchy_depths[i] == level)
                        {
                            eval_order[current_out++] = static_cast<HandleID>(i);
                        }
                    }
                }
                level_offsets.Add(static_cast<uint32_t>(current_out)); // 哨兵：level L 范围为 [offsets[L], offsets[L+1])

                topology_dirty = false;
            }

            uint16_t GetHierarchyDepth(HandleID id) const
            {
                if (id < static_cast<HandleID>(hierarchy_depths.GetCount()))
                    return hierarchy_depths[id];
                return 0;
            }

            uint32_t GetLevelCount() const
            {
                return level_offsets.GetCount() > 1 ? static_cast<uint32_t>(level_offsets.GetCount() - 1) : 0;
            }

            uint32_t GetLevelOffset(uint32_t level) const
            {
                if (level < GetLevelCount())
                    return level_offsets[level];
                return 0;
            }

            uint32_t GetLevelNodeCount(uint32_t level) const
            {
                if (level < GetLevelCount())
                    return level_offsets[level + 1] - level_offsets[level];
                return 0;
            }

            /// 纯平铺数组无递归世界矩阵计算（零指针跳转，对齐未来 ComputeShader）
            void UpdateAllWorldMatricesFlat()
            {
                if (topology_dirty)
                    RebuildTopologyOrder();

                const int count = eval_order.GetCount();
                for (int i = 0; i < count; ++i)
                {
                    const HandleID idx = eval_order[i];

                    if (local_dirty[idx])
                        UpdateLocalMatrix(idx);

                    const HandleID p = parent_indices[idx];
                    if (p != INVALID_HANDLE && p < static_cast<HandleID>(world_matrices.GetCount()))
                    {
                        world_matrices[idx] = world_matrices[p] * local_matrices[idx];
                    }
                    else
                    {
                        world_matrices[idx] = local_matrices[idx];
                    }
                    matrixDirty[idx] = 0;
                }
            }

            /// 增量脏节点平铺计算
            void UpdateDirtyWorldMatricesFlat(const std::function<void(HandleID)>& on_updated = nullptr)
            {
                if (topology_dirty)
                    RebuildTopologyOrder();

                const int count = eval_order.GetCount();
                for (int i = 0; i < count; ++i)
                {
                    const HandleID idx = eval_order[i];
                    const HandleID p = parent_indices[idx];

                    const bool parent_dirty = (p != INVALID_HANDLE && p < static_cast<HandleID>(matrixDirty.GetCount()))
                                            ? (matrixDirty[p] != 0) : false;

                    if (local_dirty[idx] || matrixDirty[idx] || parent_dirty)
                    {
                        if (local_dirty[idx])
                            UpdateLocalMatrix(idx);

                        if (p != INVALID_HANDLE && p < static_cast<HandleID>(world_matrices.GetCount()))
                        {
                            world_matrices[idx] = world_matrices[p] * local_matrices[idx];
                        }
                        else
                        {
                            world_matrices[idx] = local_matrices[idx];
                        }
                        matrixDirty[idx] = 1; // 传导至子节点

                        if (on_updated)
                            on_updated(idx);
                    }
                }

                for (int i = 0; i < count; ++i)
                {
                    matrixDirty[eval_order[i]] = 0;
                }
            }

        public: // Batch data accessors（只保留有消费者的；0 引用的读取口已删）

            const hgl::ValueArray<glm::mat4>& GetLocalMatrices() const { return local_matrices; }

        public:

            size_t GetSize() const { return static_cast<size_t>(positions.GetCount()); }
            int GetCount() const { return positions.GetCount(); }

            void Clear()
            {
                positions.Clear();
                rotations.Clear();
                scales.Clear();
                local_matrices.Clear();
                parent_indices.Clear();
                world_matrices.Clear();
                hierarchy_depths.Clear();
                eval_order.Clear();
                level_offsets.Clear();
                local_dirty.Clear();
                matrixDirty.Clear();
                mobility.Clear();
                owners.Clear();
                change_masks.Clear();
                versions.Clear();
                write_armed.Clear();
                write_warned.Clear();
                children.clear();
                entity_rows.clear();
                topology_dirty = true;
            }
        };
    }//namespace ecs
}//namespace hgl

