#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <hgl/type/ValueArray.h>
#include <hgl/ecs/core/EntityHandle.h>
#include <hgl/log/Log.h>
#include <functional>
#include <array>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace hgl
{
    namespace graph
    {
        struct CameraInfo;
        struct ViewportInfo;
    }

    namespace ecs
    {
        class ECSContext;

        /**
         * Optimized SOA (Structure of Arrays) storage for transform data
         * Provides flat layout (parent_indices, world_matrices; local matrix is composed on the fly)
         * with level-by-level topological order for object-independent evaluation
         * (ready for future ComputeShader migration).
         */
        class TransformDataStorage
        {
        public:

            using HandleID = uint32_t;
            static constexpr HandleID INVALID_HANDLE = UINT32_MAX;

            /// 声明式行预算默认值（T10/B1a；P-2 拍板 2026-10-02：static 4096 + dynamic 4096）
            static constexpr uint32_t kDefaultMaxStaticRows  = 4096;
            static constexpr uint32_t kDefaultMaxDynamicRows = 4096;

        private:

            /// 世界反向引用（**非拥有**；由所属 `ECSContext` 构造期 `SetContext(this)` 回填，
            /// 与 `VisibilityDataStorage` 同范式）。目的：任意持有本存储的代码都能反查世界，
            /// 行句柄（`TransformAccessor`）因此**不必在每个实例里重复携带 8 字节 context 指针**。
            /// 注意：本存储允许独立存在（探针 `ProbeTransformDiagnostics` / 单元测试里没有世界）⇒
            /// 所有消费点**必须容忍空**，不得假设非空。
            ECSContext *context = nullptr;

            // ── 核心数据（对应未来 GPU Storage Buffer）──
            // 局部矩阵**不落内存**：TRS 是唯一真源，local 由 `ComposeLocalMatrix()` 在求值循环里
            // 寄存器内合成后直接乘父世界矩阵（省掉每行 64 B 常驻缓存 + 第二级标脏）。
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
            hgl::ValueArray<uint8_t>        matrixDirty;        // 1 byte each (世界矩阵是否脏；单级标脏)
            hgl::ValueArray<uint8_t>        mobility;           // 1 byte each (0=static, 1=movable)

            /// 子节点表（与行一一对应；CPU 侧层级查询用）
            /// 子节点表（CSR：每行的子节点 = `child_pool[child_offset[id] .. + child_count[id])`）
            /// B2：旧实现 `std::vector<std::vector<HandleID>>` 每行固定 32 B 且绝大多数为空 ⇒
            /// 改成"每行 8 B（offset+count）+ 只在真有子节点时增长"的共享池。
            hgl::ValueArray<uint32_t>       child_offset;
            hgl::ValueArray<uint32_t>       child_count;
            std::vector<HandleID>           child_pool;

            /// **静态段变更序号**（审计 · 批 1.1 B4）：与 per-row `matrixDirty` **解耦**的静态失效信号。
            /// 为什么单开一个：`TransformSystem::Update()` 开头的 `UpdateDirtyWorldMatricesFlat()`
            /// 会先把 `matrixDirty` 清掉，于是 `SubmitTransformUpdates()` 里所有基于 dirty 的判据在
            /// 正常帧序（Update → Submit）下恒假 ⇒ 静态场景 revision 永不递增、静态级联阴影缓存不失效。
            /// 本序号只增不减，语义 = 「自上次上报以来静态集合/静态值是否变过」。
            uint64_t static_change_seq = 0;

            /// 表现层状态：fixed-pixel 尺寸控制（gizmo 等"屏幕上恒定大小"的对象）。
            /// CPU 侧、不参与世界矩阵求值 ⇒ 用普通结构数组（不是 GPU 数据）。
            struct FixedPixelState
            {
                bool  enabled = false;
                float diameter = 160.0f;
                float reference_world_diameter = 1.0f;
                float min_scale = 0.01f;
                const hgl::graph::CameraInfo   *camera_info = nullptr;
                const hgl::graph::ViewportInfo *viewport_info = nullptr;
            };

            /// 表现层状态（fixed-pixel 缩放）改**稀疏侧表**：只有被 gizmo 配置过的行才占位
            /// （旧实现 32 B/行 × 全表 ⇒ 全场景为一个 gizmo 付账）。
            std::unordered_map<HandleID,FixedPixelState> fixed_pixel_map;

            // ── 变换元数据（T8：真源在存储侧，单一真源）──
            hgl::ValueArray<EntityID>       owners;             // 所属实体（accessor 解析 owner / 告警里的实体名）
            hgl::ValueArray<uint32_t>       change_masks;       // 变更位掩码（TouchChange 累积）
            hgl::ValueArray<uint64_t>       versions;           // 变更计数（渲染侧比对"是否已上传"）
            hgl::ValueArray<uint32_t>       generations;        // 行世代：释放/重分配时 +1 ⇒ 旧句柄读时失效（v2 §2 句柄 revision 校验）

            /// §2 #9（2026-10-11）：行世代的**发放计数器**（单调，跨 `Clear()` **不复位**）。
            /// 为什么需要：`Clear()` 会清空 `generations` 数组，若新行仍从常量 1 起编号，
            /// 则"同一行号 + 世代同为 1"的旧句柄会**复活**（`TransformAccessor` 的世代校验失效）。
            /// 单调计数器保证"同一行号的新主人世代必不同"。
            uint32_t next_row_generation = 1;
            hgl::ValueArray<uint8_t>        write_armed;        // D4：该行已被渲染侧消费过
            hgl::ValueArray<uint8_t>        write_warned;       // D4：已就"运行期写静态"告警过一次

            /// 实体 → 变换行（反向索引）。T8 起"某实体有没有变换"不再靠组件查询。
            std::unordered_map<EntityID,HandleID> entity_rows;

            // ── 声明式行预算（T10/B1a）──
            // 正式形态只有"声明容量 + 预分配"：`Initialize` 一次性预留容量，
            // `Allocate` 超过预算即**拒绝分配**并**一次性明确告警**——不静默扩容
            // （与材质行 arena "上限不扩容"同一口径）。
            uint32_t  max_static_rows     = kDefaultMaxStaticRows;
            uint32_t  max_dynamic_rows    = kDefaultMaxDynamicRows;
            uint32_t  row_overflow_count  = 0;      ///< 超预算分配次数（可观测；契约用）
            bool      row_overflow_warned = false;  ///< 一次性告警闩

        public:

            /// 构造即按默认预算初始化（P-2 拍板 2026-10-02：static 4096 + dynamic 4096）。
            /// 需要别的预算的世界在创建后显式再调一次 `Initialize(...)`。
            TransformDataStorage() { Initialize(); }

            /// 声明式预算 + 预分配。行号仍 = 追加序（本步**不改行号含义**，只把容量钉死）。
            /// 目前只有 Transform 一类行存储 ⇒ 预算就是这一个世界的 (static, dynamic) 两份。
            void Initialize(const uint32_t static_rows  = kDefaultMaxStaticRows,
                            const uint32_t dynamic_rows = kDefaultMaxDynamicRows)
            {
                max_static_rows     = static_rows;
                max_dynamic_rows    = dynamic_rows;
                row_overflow_count  = 0;
                row_overflow_warned = false;

                const int budget = static_cast<int>(GetRowBudget());

                positions.Reserve(budget);
                rotations.Reserve(budget);
                scales.Reserve(budget);
                parent_indices.Reserve(budget);
                world_matrices.Reserve(budget);
                hierarchy_depths.Reserve(budget);
                eval_order.Reserve(budget);
                matrixDirty.Reserve(budget);
                mobility.Reserve(budget);
                owners.Reserve(budget);
                change_masks.Reserve(budget);
                versions.Reserve(budget);
                generations.Reserve(budget);
                write_armed.Reserve(budget);
                write_warned.Reserve(budget);
                child_offset.Reserve(budget);
                child_count.Reserve(budget);
                child_pool.reserve(static_cast<size_t>(budget));
            }

            uint32_t GetStaticRowBudget()  const { return max_static_rows; }
            uint32_t GetDynamicRowBudget() const { return max_dynamic_rows; }
            uint32_t GetRowBudget()        const { return max_static_rows + max_dynamic_rows; }
            uint32_t GetRowOverflowCount() const { return row_overflow_count; }
            bool     HasWarnedRowOverflow() const { return row_overflow_warned; }

            /// 世界反向引用（见成员注释）。本存储独立存在时（探针/单测）为空 ⇒ 消费点必须容忍空。
            void        SetContext(ECSContext *ctx) { context = ctx; }
            ECSContext *GetContext() const          { return context; }

            /// Allocate space for a new transform
            /// 预算超限 ⇒ **拒绝分配**（返回 INVALID_HANDLE）并一次性明确告警；不扩容。
            HandleID Allocate()
            {
                if (static_cast<uint32_t>(positions.GetCount()) >= GetRowBudget())
                {
                    ++row_overflow_count;

                    if (!row_overflow_warned)
                    {
                        row_overflow_warned = true;
                        GLogError("[TransformDataStorage] 变换行预算超限：budget=%u（static=%u + dynamic=%u）"
                                  " —— 预算没调够，本行**拒绝分配**（不扩容；固化 ID/存档会失效）。",
                                  GetRowBudget(), max_static_rows, max_dynamic_rows);
                    }

                    return INVALID_HANDLE;
                }

                HandleID id = static_cast<HandleID>(positions.GetCount());
                positions.Add(glm::vec3(0.0f));
                rotations.Add(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                scales.Add(glm::vec3(1.0f));

                parent_indices.Add(INVALID_HANDLE);
                world_matrices.Add(glm::mat4(1.0f));

                hierarchy_depths.Add(0);
                matrixDirty.Add(1);
                mobility.Add(1);  // Default to movable

                owners.Add(EntityID());
                change_masks.Add(0);
                versions.Add(0);
                generations.Add(next_row_generation++); // §2 #9：世代**单调**发放（跨 Clear 不复位）⇒ 旧句柄不复活
                write_armed.Add(0);
                write_warned.Add(0);
                child_offset.Add(0);
                child_count.Add(0);

                topology_dirty = true;

                // 批 1.1 B4：新行 = 场景结构变化（它可能立刻成为静态 caster）⇒ 保守触发一次静态失效
                ++static_change_seq;

                return id;
            }

            /// Free allocated space
            void Deallocate(HandleID id)
            {
                if (id >= static_cast<HandleID>(positions.GetCount()))
                    return;

                // 批 1.1 B4：行消失也是静态集合变化（静态 caster 被销毁必须让静态缓存失效）
                ++static_change_seq;

                // 失效机制（v2 §2）：释放即换代 ⇒ 任何持有旧世代的行句柄立刻作废。
                // 世代编码约定：**0 = 死/未分配；正奇数 = 活**。
                // 将来 T10 引入行复用时：复用必须在既有世代上 **+2 保持奇数**（ABA 免疫，直到 2^31 次复用）。
                if (id < static_cast<HandleID>(generations.GetCount()))
                    generations[id] = 0;

                // Mark as unused but keep slot to preserve stable indices
                parent_indices[id] = INVALID_HANDLE;
                matrixDirty[id] = 0;

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

                // 子表：本行 span 作废（池里那几格成为垃圾，不回收——子表只在装配期增长）
                if (id < static_cast<HandleID>(child_count.GetCount()))
                    child_count[id] = 0;

                // 表现层状态是稀疏侧表 ⇒ 释放即摘除条目
                fixed_pixel_map.erase(id);

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
                matrixDirty[id] = 1;
            }

            void SetLocalTRS(HandleID id, const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale)
            {
                positions[id] = pos;
                rotations[id] = rot;
                scales[id] = scale;
                matrixDirty[id] = 1;
            }

        public: // Local matrix（派生量：按需瞬时合成，**不落内存**）

            /// TRS → 局部矩阵（M = T * R * S），全程寄存器局部变量。
            /// 求值循环算完立刻乘父世界矩阵 ⇒ 不再需要第二份 64 B 常驻缓存。
            /// 只读接口不另开：引擎侧无人读局部矩阵，需要它的调用点（示例）自己从 TRS 现算。
            static glm::mat4 ComposeLocalMatrix(const glm::vec3 &pos,const glm::quat &rot,const glm::vec3 &scale)
            {
                glm::mat4 local = glm::mat4_cast(rot);

                local[0] *= scale.x;                    // 直接缩放列向量 ⇒ 省一次 scale 矩阵乘
                local[1] *= scale.y;
                local[2] *= scale.z;
                local[3] = glm::vec4(pos,1.0f);

                return local;
            }

        public: // World matrix accessors

            glm::mat4 GetWorldMatrix(HandleID id) const
            {
                return world_matrices[id];
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

                // 批 1.1 B4：静态行（mobility 0）的值变更 ⇒ 静态集合/静态值变了
                if (id < static_cast<HandleID>(mobility.GetCount()) && mobility[id] == 0)
                    ++static_change_seq;
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

            /// 静态变更序号（批 1.1 B4）：消费者按「与上次上报的值是否相同」判定静态场景是否变更
            uint64_t GetStaticChangeSeq() const { return static_change_seq; }

            /// 行世代（句柄失效检测）。越界或从未分配返回 0 —— 默认构造的句柄世代也是 0 ⇒ 不会误判为有效。
            uint32_t GetGeneration(HandleID id) const
            {
                if (id >= static_cast<HandleID>(generations.GetCount()))
                    return 0;

                return generations[id];
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

        public: // 每行字节账目（探针 / 测试共用；**新增每行数组时只改这一处**）

            /// 一条平行数组的账目
            struct PerRowField
            {
                const char *name;
                uint32_t    bytes;   ///< 该数组每行占的字节（= 元素大小）
                uint32_t    count;   ///< 元素数（不变量：恒等于 GetCount()）
            };

            static constexpr uint32_t PER_ROW_FIELD_COUNT = 17;

            using PerRowFieldTable = std::array<PerRowField,PER_ROW_FIELD_COUNT>;

            /// 逐条列出每行数组。**新增/删除每行数组时同步改这里**，探针与测试都跟着走。
            /// 不含 `level_offsets`（每 Level 一条，不是每行）与 `entity_rows`（哈希表，堆开销由分配统计覆盖）。
            PerRowFieldTable GetPerRowFields() const
            {
                return { {
                    { "parent",       static_cast<uint32_t>(sizeof(HandleID)),         static_cast<uint32_t>(parent_indices.GetCount()) },
                    { "world_mat4",   static_cast<uint32_t>(sizeof(glm::mat4)),        static_cast<uint32_t>(world_matrices.GetCount()) },
                    { "depth",        static_cast<uint32_t>(sizeof(uint16_t)),         static_cast<uint32_t>(hierarchy_depths.GetCount()) },
                    { "eval_order",   static_cast<uint32_t>(sizeof(HandleID)),         static_cast<uint32_t>(eval_order.GetCount()) },
                    { "positions",    static_cast<uint32_t>(sizeof(glm::vec3)),        static_cast<uint32_t>(positions.GetCount()) },
                    { "rotations",    static_cast<uint32_t>(sizeof(glm::quat)),        static_cast<uint32_t>(rotations.GetCount()) },
                    { "scales",       static_cast<uint32_t>(sizeof(glm::vec3)),        static_cast<uint32_t>(scales.GetCount()) },
                    { "matrix_dirty", static_cast<uint32_t>(sizeof(uint8_t)),          static_cast<uint32_t>(matrixDirty.GetCount()) },
                    { "mobility",     static_cast<uint32_t>(sizeof(uint8_t)),          static_cast<uint32_t>(mobility.GetCount()) },
                    { "owners",       static_cast<uint32_t>(sizeof(EntityID)),         static_cast<uint32_t>(owners.GetCount()) },
                    { "change_masks", static_cast<uint32_t>(sizeof(uint32_t)),         static_cast<uint32_t>(change_masks.GetCount()) },
                    { "versions",     static_cast<uint32_t>(sizeof(uint64_t)),         static_cast<uint32_t>(versions.GetCount()) },
                    { "generation",   static_cast<uint32_t>(sizeof(uint32_t)),         static_cast<uint32_t>(generations.GetCount()) },
                    { "write_armed",  static_cast<uint32_t>(sizeof(uint8_t)),          static_cast<uint32_t>(write_armed.GetCount()) },
                    { "write_warned", static_cast<uint32_t>(sizeof(uint8_t)),          static_cast<uint32_t>(write_warned.GetCount()) },
                    { "child_offset", static_cast<uint32_t>(sizeof(uint32_t)),         static_cast<uint32_t>(child_offset.GetCount()) },
                    { "child_count",  static_cast<uint32_t>(sizeof(uint32_t)),         static_cast<uint32_t>(child_count.GetCount()) },
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

            /// 其中"派生/缓存"部分（只剩 world_mat4：局部矩阵改为按需合成、不常驻）—— 报告里单列，便于讨论去留
            uint32_t DerivedRowBytes() const
            {
                return static_cast<uint32_t>(sizeof(glm::mat4));
            }

            /// 不变量检查：每个每行数组的元素数都必须等于行数。
            /// 返回第一个不匹配的字段名（全一致返回 nullptr）——漏在 Allocate/Deallocate 里同步的数组会在这里现形。
            /// 注意：`eval_order` / `hierarchy_depths` 由 `RebuildTopologyOrder()` 按需重建，
            ///       检查前应先求值一次（`UpdateDirtyWorldMatricesFlat()`），否则会在"拓扑未结算"时误报。
            const char *FindPerRowCountMismatch() const
            {
                const uint32_t rows = static_cast<uint32_t>(GetCount());

                for (const PerRowField &f : GetPerRowFields())
                    if (f.count != rows)
                        return f.name;

                return nullptr;
            }

        public: // 表现层状态：fixed-pixel 尺寸控制（真源在存储）

            FixedPixelState GetFixedPixel(HandleID id) const
            {
                const auto it = fixed_pixel_map.find(id);

                return (it != fixed_pixel_map.end()) ? it->second : FixedPixelState();
            }

            void SetFixedPixel(HandleID id,const FixedPixelState &state)
            {
                if (id >= static_cast<HandleID>(GetCount()))
                    return;

                fixed_pixel_map[id] = state;
            }

        public: // 子节点（CPU 侧层级查询；GPU 侧走 parent_indices + eval_order，与此无关）
                 // B2：CSR —— 每行只占 `child_offset` + `child_count` 共 8 B（旧实现 32 B/行）。

            uint32_t GetChildCount(HandleID id) const
            {
                if (id >= static_cast<HandleID>(child_count.GetCount()))
                    return 0;

                return child_count[id];
            }

            HandleID GetChild(HandleID id,uint32_t index) const
            {
                if (index >= GetChildCount(id))
                    return INVALID_HANDLE;

                return child_pool[child_offset[id] + index];
            }

            bool HasChild(HandleID id,HandleID child) const
            {
                const uint32_t count = GetChildCount(id);

                if (count == 0)
                    return false;

                const uint32_t offset = child_offset[id];

                for (uint32_t i = 0; i < count; ++i)
                    if (child_pool[offset + i] == child)
                        return true;

                return false;
            }

            /// 拷出某行的子节点快照（不返回引用 ⇒ 池重排后不会悬垂）
            void CopyChildren(HandleID id,std::vector<HandleID> &out) const
            {
                out.clear();

                const uint32_t count = GetChildCount(id);

                if (count == 0)
                    return;

                const uint32_t offset = child_offset[id];
                out.assign(child_pool.begin() + offset,child_pool.begin() + offset + count);
            }

            void AddChild(HandleID id,HandleID child)
            {
                if (id >= static_cast<HandleID>(child_count.GetCount()) || child == id)
                    return;

                if (HasChild(id,child))
                    return;

                const uint32_t offset = child_offset[id];
                const uint32_t count  = child_count[id];

                // 本行 span 恰好贴着池尾 ⇒ 直接续写（装配期常见路径，零浪费）；
                // 否则把本行整段**复制**到池尾（旧 span 成为垃圾，不回收——子表只在装配期增长）。
                //
                // 2026-10-11（审计 · 批 1.1 B1）修复：原实现只把 `child_offset` 重指到池尾却**不搬旧子节点**，
                // 随后照样写 `count + 1` ⇒ 区间前半是新子节点、其后是别人的数据；`CopyChildren`/`HasChild`
                // 还会按 `[offset, offset+count)` 读出 `child_pool` 之外（UB）。触发面 = 任意
                // 「父 X 先有子、另一父 Y 也有子、X 再加子」的装配顺序（普通层级装配即可命中）。
                if (count != 0 && (offset + count) != static_cast<uint32_t>(child_pool.size()))
                {
                    const uint32_t new_offset = static_cast<uint32_t>(child_pool.size());

                    // 先保容量：insert 的源区间就在同一 vector 内，一旦重分配迭代器即失效
                    child_pool.reserve(child_pool.size() + static_cast<size_t>(count) + 1u);

                    child_pool.insert(child_pool.end(),
                                      child_pool.begin() + offset,
                                      child_pool.begin() + offset + count);

                    child_offset[id] = new_offset;
                }
                else if (count == 0)
                {
                    child_offset[id] = static_cast<uint32_t>(child_pool.size());
                }

                child_pool.push_back(child);
                child_count[id] = count + 1;
            }

            void RemoveChild(HandleID id,HandleID child)
            {
                const uint32_t count = GetChildCount(id);

                if (count == 0)
                    return;

                const uint32_t offset = child_offset[id];

                for (uint32_t i = 0; i < count; ++i)
                {
                    if (child_pool[offset + i] != child)
                        continue;

                    // 行内前移，保持区间连续（span 仍为 [offset, offset + count - 1)）
                    for (uint32_t j = i; j + 1 < count; ++j)
                        child_pool[offset + j] = child_pool[offset + j + 1];

                    child_count[id] = count - 1;
                    return;
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
                // 越界 / 空洞（INVALID_HANDLE）⇒ 视为不脏 + **一次性报错**。
                // 名单里的空洞是设计（Context.h：空洞记 INVALID_TRANSFORM_ID，注册优先复用）；
                // 但拿空洞查询行存储 = 调用方漏了跳洞 ⇒ 以 (uint32)-1 索引平行数组会直接越界。
                // 2026-10-02 实测崩溃栈：TransformSystem::Update → IsDirty(4294967295) →
                // ValueArray<unsigned char>::operator[](-1) → std::vector OOB。
                if (id >= static_cast<HandleID>(matrixDirty.GetCount()))
                {
                    static bool s_warned_invalid = false;
                    if (!s_warned_invalid)
                    {
                        s_warned_invalid = true;
                        GLogError("[TransformDataStorage] IsDirty 收到无效句柄 id=%u（空洞 INVALID_HANDLE=%u / 越界 rows=%u）"
                                  "—— 名单遍历必须走 ECSContext::ForEach*Transform（内部跳洞），不要裸遍历名单",
                                  id, static_cast<uint32_t>(INVALID_HANDLE), static_cast<uint32_t>(matrixDirty.GetCount()));
                    }
                    return false;
                }

                return matrixDirty[id] != 0;
            }

            void SetDirty(HandleID id, bool dirty)
            {
                matrixDirty[id] = dirty ? 1 : 0;

                // 批 1.1 B4：静态行被标脏（含只标脏不 bump version 的路径）同样算静态变更
                if (dirty && id < static_cast<HandleID>(mobility.GetCount()) && mobility[id] == 0)
                    ++static_change_seq;
            }

        public: // Mobility tracking (0 = static, 1 = movable)

            uint8_t GetMobility(HandleID id) const
            {
                return mobility[id];
            }

            void SetMobility(HandleID id, uint8_t mobilityValue)
            {
                const bool changed = (id < static_cast<HandleID>(mobility.GetCount()))
                                   && (mobility[id] != mobilityValue);

                mobility[id] = mobilityValue;

                // 批 1.1 B4：换段 = 静态集合变化（进静态 / 出静态都要让静态缓存失效）
                if (changed)
                    ++static_change_seq;
            }

        public: // Flat arrays accessors (for GPU uploads / ComputeShader dispatch)

            const HandleID* GetParentIndicesData() const { return parent_indices.GetData(); }
            const glm::mat4* GetWorldMatricesData() const { return world_matrices.GetData(); }
            glm::mat4* GetWorldMatricesData() { return world_matrices.GetData(); }

            const HandleID* GetEvalOrderData() const { return eval_order.GetData(); }

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
                    const HandleID p   = parent_indices[idx];

                    // 局部矩阵当场合成（寄存器局部变量），不再写回常驻数组
                    const glm::mat4 local = ComposeLocalMatrix(positions[idx],rotations[idx],scales[idx]);

                    if (p != INVALID_HANDLE && p < static_cast<HandleID>(world_matrices.GetCount()))
                    {
                        world_matrices[idx] = world_matrices[p] * local;
                    }
                    else
                    {
                        world_matrices[idx] = local;
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

                    // ⚠️ `parent_dirty` 不能省：存储层不做子树标脏（Set* 只写自己一行），
                    //    "父动子跟" 只能靠这里传播（Test 5 就是这条路的回归门）。
                    const bool parent_dirty = (p != INVALID_HANDLE && p < static_cast<HandleID>(matrixDirty.GetCount()))
                                            ? (matrixDirty[p] != 0) : false;

                    if (matrixDirty[idx] || parent_dirty)
                    {
                        const glm::mat4 local = ComposeLocalMatrix(positions[idx],rotations[idx],scales[idx]);

                        if (p != INVALID_HANDLE && p < static_cast<HandleID>(world_matrices.GetCount()))
                        {
                            world_matrices[idx] = world_matrices[p] * local;
                        }
                        else
                        {
                            world_matrices[idx] = local;
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

        public:

            size_t GetSize() const { return static_cast<size_t>(positions.GetCount()); }
            int GetCount() const { return positions.GetCount(); }

            /// 整表复位（行数据 + 全部平行数组）。
            /// §2 #9（2026-10-11）：**世代计数器不复位**（见 `next_row_generation`）⇒ 复位后
            /// 新行拿到的世代与旧行必不同，旧句柄不会复活；调用点 = `ECSContext::Shutdown`
            /// （此前它零调用者 ⇒ Shutdown 清了实体却不清行存储 ⇒ 新实体继承旧变换行）。
            void Clear()
            {
                positions.Clear();
                rotations.Clear();
                scales.Clear();
                parent_indices.Clear();
                world_matrices.Clear();
                hierarchy_depths.Clear();
                eval_order.Clear();
                level_offsets.Clear();
                matrixDirty.Clear();
                mobility.Clear();
                owners.Clear();
                change_masks.Clear();
                versions.Clear();
                generations.Clear();
                write_armed.Clear();
                write_warned.Clear();
                child_offset.Clear();
                child_count.Clear();
                child_pool.clear();
                fixed_pixel_map.clear();
                entity_rows.clear();
                topology_dirty = true;
            }
        };
    }//namespace ecs
}//namespace hgl

