#pragma once
// PrimitiveTopologyLedger.h —— 「主帧图元收集/合批缓存」的世界私有失效台帐（计划 P1）
//
// 为什么需要它：`RenderFrameCache::BeginFrame()` 每个 pass 都清空收集产出的 item 与批，
// 所以「只变 TRS 也要每帧全量重收集」是当前架构的必然结果。要跳过重复工作，就必须先能
// **证明**拓扑（成员集合 + 材质/几何/模板绑定 + 批划分）没有变 —— 而证明的前提是把
// 所有会改变它的写入口找齐、并给它们一个可比较的代数。
//
// 本台帐只做两件事，**不做任何跳过决策**（P1 的边界）：
//   1. 单调代数：每类输入一个计数器，写入方调用 `MarkTopology/MarkDynamicFact`；
//   2. 设备级行池代数轮询：`PollDeviceRowPools()` 把 `ActiveRowPool` 的
//      「行占用代数 / 行内容代数」折算进同一套计数器，覆盖那些**无法**逐个加固
//      写入口的全局池（材质行、模板池、部件池、网格绘制参数行…）。
//
// 为什么轮询也可靠：池的行号分配/归还/内容提交**只**经 `ActiveRowPool` 的少数几个
// 方法，代数在那里自增是**单点**保证；轮询只是把「设备级的真值」读进世界私有台帐，
// 不需要号召所有写入方记得通知。反过来，「世界侧」的变更（实体增删、组件挂卸、
// GeometryData 换资产、MaterialData 授权代前进）无法从池代数推出，必须显式 Mark。
//
// 已知**未封闭**的写入方（P1 保留拒绝，不假装覆盖）：
//   · `GPUModelRefComponent` 的裸字段（`part_visibility_mask` / `custom_data_index` /
//     `prev_transform_id`）—— 任何人都能直接写，无法拦截。这些字段的消费者目前是
//     collect 的「本项进批」判据点（每帧按值比对后写 `SetModelFacts`），所以它们是
//     **每帧动态事实**而非拓扑；P3 的快路径必须继续对全部命中项重新发布，不能靠脏集。
//   · `PrimitiveAsset` / `GeometryAsset` 内部的状态迁移（`MetaOnly`→`Resident` 等）——
//     引擎的设计是「每帧现算 `CanRender`、不缓存」，故不走台帐：它们改的是
//     **本帧能不能画**，而它由每帧的判据循环重新求值。
//
// 线程约定：只在主线程读写（与其它世界级存储一致）。

#include<cstddef>
#include<cstdint>

namespace hgl
{
    namespace graph
    {
        class GlobalSSBOBufferRegistry;
    }
}

namespace hgl::ecs
{
    /**
     * @brief 拓扑变更类别 —— 计划 §3.2 输入表的可执行划分。
     *
     * 「拓扑」= 能被缓存复用的一切结构：成员集合、图元↔几何/材质/模板绑定、批与桶划分。
     * 纯 TRS 与相机**不在**此列（它们每帧照常更新）。
     */
    enum class TopologyChange : uint8_t
    {
        EntityStructure = 0,   ///< 实体创建/销毁、组件挂卸、ID 回收（含 generation 推进）
        GeometryBinding,       ///< GeometryData 换资产 / 换变体 / 几何行号变化
        ModelTemplate,         ///< 模型模板注册/注销/代理回填、部件行区间变化
        MaterialBinding,       ///< 配方/授权/参数行/shader 身份/管线缓存失效
        PassAndTarget,         ///< render-pass / RT / pipeline / resize / 场景 profile 变化
        Count
    };

    /**
     * @brief 动态事实变更类别 —— 不改拓扑、但必须重新发布到 GPU 行的输入。
     *
     * 与 `TopologyChange` 分开记的理由：动态事实可以让**同一批** item/批继续复用，
     * 只要在派发前把新事实写进 `EntityGPUTable` / 4-ID 表。混在一起记会把「改个
     * 可见性」升级成「重建全部批」，那是过度失效。
     */
    enum class DynamicFactChange : uint8_t
    {
        Visibility = 0,   ///< 自身或祖先可见性变化
        PartMask,         ///< `GPUModelRefComponent::part_visibility_mask` 变化
        CustomData,       ///< 实例自定义数据行变化
        PrevTransform,    ///< 前帧变换行变化
        MaterialContent,  ///< 材质字段行的**内容**（行号不变）变化
        Count
    };

    /**
     * @brief 主帧缓存资格拒绝原因。
     *
     * 顺序即判定顺序：越靠前越是「连候选都不是」的硬条件。P1 只统计不跳过，
     * 所以这些值是**观测量**，不是行为开关。
     */
    enum class CacheRejectReason : uint8_t
    {
        Eligible = 0,             ///< 通过全部判据（P1 中**不应**出现，因为还没有快照）
        NotMainSwapchainFrame,    ///< 非交换链主帧（阴影/离屏/RenderTo）
        GpuCullNotEngaged,        ///< GPU 剔除未接管（CPU 路径仍需要完整收集）
        NoSnapshot,               ///< 尚无主帧拓扑快照（P2 之前恒为真）
        SnapshotNotReady,         ///< 快照处于 Building/Invalid
        TopologyDirty,            ///< 本帧或上次发布以来有拓扑变更
        Count
    };

    /**
     * @brief 资格判定的输入快照（由调用方按当前帧实际情况填写）。
     *
     * 刻意全是标量：判定函数不碰世界、不碰 GPU，便于单测和对拍。
     */
    struct CollectCacheInputs
    {
        bool     is_main_swapchain_frame = false;  ///< 交换链主帧 + 正常主相机 + 无额外过滤
        bool     gpu_cull_engaged        = false;  ///< 本帧 GPU 剔除确实接管了绘制
        bool     has_snapshot            = false;  ///< 存在可用快照对象（P2 之前恒 false）
        bool     snapshot_ready          = false;  ///< 快照状态 == Ready
        uint64_t snapshot_topology_rev   = 0;      ///< 快照发布时的拓扑总代数
    };

    /**
     * @brief 世界私有的失效台帐。
     *
     * 一个世界一份（`ECSContext` 成员）；多世界互不污染。全部状态是 POD 计数器 +
     * 一张 10 项的池代数表，无堆分配。
     */
    class PrimitiveTopologyLedger
    {
    public:

        /// 池代数表的固定宽度。取 `hgl::graph::GlobalSSBOTypeCount` 的编译期上界，
        /// 但**不**在本头引入 `GlobalSSBOTypes.h`（它拖 EnumUtil/String/EnumMeta 进来，
        /// 而本头会被组件头间接包含）。`.cpp` 里有 static_assert 守住这个上界够用。
        static constexpr size_t kPoolSlotCount = 16;

    private:

        /// 每类拓扑变更的代数（只增不减）
        uint64_t topology_rev[static_cast<size_t>(TopologyChange::Count)] = {};

        /// 拓扑总代数 = 各类别之和。消费者只需比较**一个数**即可判「有没有变」，
        /// 而逐类别值保留下来用于回答「是谁变的」。
        uint64_t topology_total = 0;

        /// 每类动态事实的代数
        uint64_t fact_rev[static_cast<size_t>(DynamicFactChange::Count)] = {};

        /// 动态事实总代数
        uint64_t fact_total = 0;

        /// 每类拓扑变更**最后一次**发生的帧序号（诊断用：报表能指出「这帧谁动了拓扑」）
        uint32_t topology_last_frame[static_cast<size_t>(TopologyChange::Count)] = {};

        /// 上一次 `PollDeviceRowPools` 见过的池代数（按 `GlobalSSBOType` 取值下标）
        uint64_t polled_layout[kPoolSlotCount]  = {};
        uint64_t polled_content[kPoolSlotCount] = {};

        bool     polled_valid = false;

        /// 上一帧合批阶段观测到的「GPU 剔除是否真的接管了绘制」。
        /// 由 `PrimitiveBatchPipeline` 回填；collect 在**下一帧**读它 —— 因为本帧的
        /// 合批还没发生，而 GPU 接管是**逐帧稳定性**条件（接管失败即 fail-fast 清空批，
        /// 那种帧根本不能复用上一帧的批布局）。
        bool     last_gpu_cull_engaged = false;

        bool     last_gpu_cull_engaged_valid = false;

        /// 世界存活过的帧序号（`BeginFrame` 自增；只用于诊断打标）
        uint32_t frame_serial = 0;

        // 观测计数（P1：只进报表，不参与任何决策）
        uint64_t stat_candidate_frames = 0;   ///< 进入资格判定的帧数
        uint64_t stat_eligible_frames  = 0;   ///< 通过全部判据的帧数
        uint64_t stat_reject[static_cast<size_t>(CacheRejectReason::Count)] = {};
        uint64_t stat_pool_layout_changes  = 0;   ///< 池轮询检出「行占用代数变」的累计次数
        uint64_t stat_pool_content_changes = 0;   ///< 池轮询检出「行内容代数变」的累计次数

    public:

        PrimitiveTopologyLedger() = default;
        ~PrimitiveTopologyLedger() = default;

        PrimitiveTopologyLedger(const PrimitiveTopologyLedger &) = delete;
        PrimitiveTopologyLedger &operator=(const PrimitiveTopologyLedger &) = delete;

    public:

        // ── 写入方接口 ────────────────────────────────────────────────────

        /// 声明「本帧发生了该类拓扑变更」。写入方在**真正改了数据之后**调用。
        void MarkTopology(TopologyChange kind, uint32_t count = 1);

        /// 声明「本帧发生了该类动态事实变更」。
        void MarkDynamicFact(DynamicFactChange kind, uint32_t count = 1);

        /**
         * @brief 把设备级行池的代数读进台帐（每帧一次，O(池数) = 10 次比较）。
         *
         * 行**占用**代数变化 ⇒ 拓扑变更（行号分配/归还改变了谁引用哪一行）；
         * 行**内容**代数变化 ⇒ 动态事实（同一行的字节变了，不影响键与容量）。
         *
         * @param registry 可为空（无设备/未初始化时）；空则跳过一次采样。
         * @return 本次采样是否检出了变化
         */
        bool PollDeviceRowPools(graph::GlobalSSBOBufferRegistry *registry);

        /// 帧序号自增并返回新值（由 collect 每帧调用一次）
        uint32_t BeginFrame() { return ++frame_serial; }
        uint32_t GetFrameSerial() const { return frame_serial; }

        /// 合批阶段回填「GPU 剔除是否接管」（供下一帧的资格判定使用）
        void ObserveGPUCullEngaged(bool engaged)
        {
            last_gpu_cull_engaged       = engaged;
            last_gpu_cull_engaged_valid = true;
        }

        bool GetLastGPUCullEngaged() const { return last_gpu_cull_engaged; }
        bool HasGPUCullObservation() const { return last_gpu_cull_engaged_valid; }

        // ── 读取方接口 ────────────────────────────────────────────────────

        uint64_t GetTopologyRevision() const { return topology_total; }
        uint64_t GetDynamicFactRevision() const { return fact_total; }

        uint64_t GetTopologyRevision(TopologyChange kind) const;
        uint64_t GetDynamicFactRevision(DynamicFactChange kind) const;
        uint32_t GetTopologyLastFrame(TopologyChange kind) const;

        /// 自上次 `Reset` 以来是否有过任何拓扑变更
        bool HasTopologyChanged() const { return topology_total != 0; }

        // ── 资格判定（P1：只观测） ────────────────────────────────────────

        /**
         * @brief 判定本帧是否**有资格**复用主帧拓扑快照。
         *
         * 判定顺序固定，返回第一个不满足的原因（便于报表直接归因）。
         * **本函数不修改任何行为** —— 调用方只累加统计，不据此跳过收集。
         */
        CacheRejectReason Evaluate(const CollectCacheInputs &in) const;

        /// 记录一次判定结果（供 `Evaluate` 调用方使用；同时推进帧级统计）
        void RecordEligibility(CacheRejectReason reason);

        uint64_t GetStatCandidateFrames() const { return stat_candidate_frames; }
        uint64_t GetStatEligibleFrames() const { return stat_eligible_frames; }
        uint64_t GetStatReject(CacheRejectReason reason) const;

        /// 池轮询检出「行占用代数变」的累计次数（稳态下应为 0；持续增长 ⇒ 有系统在
        /// 每帧分配/归还行，必须先收敛该写入方，否则 P3 的快路径会持续失效）
        uint64_t GetStatPoolLayoutChanges() const { return stat_pool_layout_changes; }

        /// 池轮询检出「行内容代数变」的累计次数（正常场景每帧都会涨：变换/材质参数），
        /// 它只影响动态事实，不否决拓扑复用。
        uint64_t GetStatPoolContentChanges() const { return stat_pool_content_changes; }

        /// 清空全部代数与统计（世界 Shutdown / 重配时调用）。
        /// ⚠ 清代数会让「上一帧发布过快照」的记录失效 ⇒ 调用方必须同时丢弃快照。
        void Reset();
    };

    /// 名字表（报表用；英文短名，与 `FrameProfiler` 的槽名风格一致）
    const char *GetTopologyChangeName(TopologyChange kind);
    const char *GetDynamicFactChangeName(DynamicFactChange kind);
    const char *GetCacheRejectReasonName(CacheRejectReason reason);
}//namespace hgl::ecs
