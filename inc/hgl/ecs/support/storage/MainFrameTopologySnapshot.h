#pragma once

#include<hgl/type/ValueArray.h>
#include<cstdint>

/**
 * 主帧拓扑快照（执行计划 P2）
 *
 * ## 目的
 *
 * 现状：`RenderFrameCache::BeginFrame()` 在**每个渲染 pass** 都清空
 * `renderItems` / `materialBatches` / `ordered_batches`，于是主帧每帧都要
 * 重新做一遍全量收集 + 合批（基线里 `CollectPhase` 占 75%）。
 *
 * P2 引入一份**主帧专属**的持久拓扑快照：把「主帧这一帧的可绘制拓扑」
 * （批集合 + 批内成员顺序 + 每个成员的实体身份与材质行号）保存下来，
 * 用 `PrimitiveTopologyLedger` 的拓扑代数做**失效判据**。P3 之后快路径即可
 * 在代数未变时直接从快照重建出等价结果，跳过收集主循环。
 *
 * ## P2 阶段的边界（严格遵守，不得越界）
 *
 * **P2 只做「构建 + 对拍 + 统计」，绝不跳过任何原始工作。**
 * - `BuildFromCache()` 对 `RenderFrameCache` **只读**，不写任何字段、不移动任何指针；
 * - 原始收集/合批/绘制路径一字不改地跑完整；
 * - 快照命中率与「拓扑未变但内容变了」（`parity_mismatch`）都只进统计与报表。
 *
 * 这样 P2 落地后可以先用「启用快照 / 不启用快照」两跑对比证明**零行为变化**，
 * 再在 P3 让快照真正接管。
 *
 * ## 所有权与寿命契约（P3 消费方必须遵守）
 *
 * 1. **只存值，不存可解引用指针**。记录里出现的 `key_identity` 是把
 *    `ShaderProgram*` / `Pipeline*` 的**指针值**当身份令牌折叠出的哈希，
 *    它**永远不得被解引用或转回指针** —— 管线对象、渲染目标、设备缓冲的寿命
 *    由各自的拥有者（`RenderSystemCore` / `RenderGraph` / `BufferManager`）管理，
 *    快照不参与、也不延长任何资源的退休时机。
 * 2. **不得缓存系统指针**。`MaterialBatch::transform_buffer` 由 `TransformSystem`
 *    持有（系统销毁即失效），`MaterialBatch` 的 ICB / 行表指针由 GPU 剔除每帧
 *    重发 —— 这些**一律不进快照**。
 * 3. **单世界隔离**。快照是 `ECSContext` 的成员 ⇒ 天然一个世界一份；不得跨世界
 *    共享，也不得用其它世界的 `RenderFrameCache` 构建。
 * 4. **主帧专属**。只有「非阴影 pass 且无移动性过滤」的 pass 才允许构建
 *    （与 P1 的资格判定同一谓词）。阴影 pass / 移动性过滤 pass / 离屏 RTT pass
 *    共用同一份 `RenderFrameCache`，成员集合天然不同，不能互相覆盖快照。
 * 5. **提交是原子的**。构建写入 pending 槽，全部成功后一次性翻转到 live 槽；
 *    live 槽的退休是**立即**的（记录是纯值，没有 GPU 资源要等 fence），
 *    下一轮构建前 pending 槽整体清空。
 * 6. **失效是显式的**。pass/target 重建（`OnResize`、`SetScenePipelineMode`）
 *    直接 `Invalidate()`；拓扑变化**不**靠即时失效，而靠代数比较（重建比提前
 *    失效便宜，且重建失败时旧快照自然因代数不符而不可用）。
 */

namespace hgl::ecs
{
    struct RenderFrameCache;

    /// 快照状态
    enum class SnapshotState : uint8_t
    {
        Uninitialized = 0,   ///< 从未成功提交过
        Ready,               ///< 已提交，可被 P3 消费
        Invalid,             ///< 已显式失效（pass/target 重建）
        Count
    };

    /// 状态名（报表用；越界返回 "<bad>"）
    const char *GetSnapshotStateName(SnapshotState state);

    /// 读取环境变量 `ULRE_MAINFRAME_TOPO_CACHE`（`1` / `true` / `on` / `yes` 视为开启）。
    /// 引擎侧默认**关闭**：P2 的快照只做观测，开启后每帧多一次全量遍历 +
    /// 一次批排序，不开就没有任何额外开销。宿主也可在创建世界后直接
    /// `SetMainFrameTopologySnapshotEnabled(true/false)` 覆盖。
    bool GetMainFrameTopologySnapshotEnvDefault();

    /// 一个可绘制成员
    struct SnapshotItemRecord
    {
        uint32_t entity_index      = 0;   ///< `EntityID::index`
        uint16_t entity_generation = 0;   ///< `EntityID::generation`（陈旧句柄以此判出）
        uint16_t reserved0         = 0;   ///< 保留（显式填零，便于逐字节对拍）
        uint32_t material_row      = 0;   ///< `MaterialRuntimeRowID`（材质运行期共享行号）
        uint32_t batch_slot        = 0;   ///< 所属批在 `SnapshotBatchRecord` 数组里的下标
    };

    /// 一个批（= 一次 `MaterialBatch`）
    struct SnapshotBatchRecord
    {
        uint64_t key_identity = 0;   ///< `FNV1a(shader_program 指针值, pipeline 指针值)`；**身份令牌，永不解引用**
        uint64_t key_config   = 0;   ///< `HashMaterialPipelineConfig(pipeline_config)`；动态渲染状态
        uint32_t first_item   = 0;   ///< 本批第一个成员在 item 数组里的下标
        uint32_t item_count   = 0;   ///< 本批成员数
    };

    /// P2 观测统计
    struct SnapshotStats
    {
        uint64_t builds          = 0;   ///< 构建次数
        uint64_t commits         = 0;   ///< 成功提交次数
        uint64_t aborts          = 0;   ///< 构建被拒绝/中断次数（P2 只应出现在重入）
        uint64_t invalidations   = 0;   ///< 显式失效次数
        uint64_t hits            = 0;   ///< 代数未变且内容哈希一致 ⇒ P3 可复用
        uint64_t misses          = 0;   ///< 代数变了 / 从未构建 / 内容不一致
        uint64_t parity_mismatch = 0;   ///< **告警**：代数未变但内容变了 = 存在未上报的写者
        uint64_t empty_builds    = 0;   ///< 构建出 0 个成员的次数（合法，主帧真的没东西可画）
        uint64_t last_items      = 0;   ///< 最近一次提交的成员数
        uint64_t last_batches    = 0;   ///< 最近一次提交的批数
    };

    /**
     * 主帧拓扑快照。
     *
     * 构建与消费都在主线程（渲染线程）内，**无任何同步原语**：它不跨线程。
     */
    class MainFrameTopologySnapshot
    {
    public:

        MainFrameTopologySnapshot() = default;
        ~MainFrameTopologySnapshot() = default;

        MainFrameTopologySnapshot(const MainFrameTopologySnapshot &) = delete;
        MainFrameTopologySnapshot &operator=(const MainFrameTopologySnapshot &) = delete;

    private:

        SnapshotState state = SnapshotState::Uninitialized;

        /// 双槽：`[live_slot]` 是已提交内容，`[pending_slot]` 是构建目标。
        /// 用双槽而不是「构建成功再 swap 两个容器」，是为了让提交只剩一次下标翻转 ——
        /// 没有任何容器搬移，也就没有搬移失败/半途异常导致的 live 内容损坏。
        ValueArray<SnapshotItemRecord>  items[2];
        ValueArray<SnapshotBatchRecord> batches[2];

        uint32_t live_slot    = 0;
        uint32_t pending_slot = 1;

        uint64_t topology_revision = 0;   ///< 构建时消耗的台帐拓扑代数
        uint64_t content_hash      = 0;   ///< 全部记录 + 计数的 FNV1a（对拍用）
        uint32_t frame_serial      = 0;   ///< 构建时的 `RenderFrameCache::frame_serial`

        bool building = false;            ///< 重入护栏

        SnapshotStats stats;

        void ClearPending();
        void ReleaseLive();

    public:

        /**
         * 从本帧的 `RenderFrameCache` 构建并（成功时）原子提交一份快照。
         *
         * 只读 `cache`：不写字段、不移动指针、不改变任何绘制结果。
         *
         * 批顺序取**规范顺序**（按 `ShaderProgramPipelineKey::operator<` 排序），
         * 与 `PrimitiveRenderSystem::RenderBatches` 的批间排序同一依据 ——
         * 刻意不用 `cache.ordered_batches`：那是 `RenderDrawSubmit` 阶段才填、
         * 且主帧/覆盖层两次调用会互相覆盖，不是稳定的批序来源。
         *
         * @return true = 已提交新快照；false = 被拒绝（重入），live 内容不变。
         */
        bool BuildFromCache(const RenderFrameCache &cache,
                            uint64_t in_topology_revision,
                            uint32_t in_frame_serial);

        /// 显式失效（pass/target 重建）。已提交内容立即作废，下次构建记 miss。
        void Invalidate();

        SnapshotState GetState() const { return state; }
        bool IsReady() const { return state == SnapshotState::Ready; }

        int GetItemCount()  const { return items[live_slot].GetCount(); }
        int GetBatchCount() const { return batches[live_slot].GetCount(); }

        const SnapshotItemRecord  *GetItems()   const { return items[live_slot].GetData(); }
        const SnapshotBatchRecord *GetBatches() const { return batches[live_slot].GetData(); }

        uint64_t GetTopologyRevision() const { return topology_revision; }
        uint64_t GetContentHash()      const { return content_hash; }
        uint32_t GetBuiltFrameSerial() const { return frame_serial; }

        const SnapshotStats &GetStats() const { return stats; }

        /// 只清统计（不动已提交内容）
        void ResetStats();

        /// 全清（内容 + 状态 + 统计）；用于世界初始化/彻底重建
        void Reset();
    };
}//namespace hgl::ecs
