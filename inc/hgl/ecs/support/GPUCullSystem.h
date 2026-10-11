#pragma once

#include <hgl/ecs/support/RenderPassSlot.h>   // 跨模块单源：pass 槽位/遍数（T4c3a）
#include <hgl/ecs/core/System.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKQueue.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/vk/buffer/IndirectCommandBuffer.h>
#include <hgl/vk/pipeline/VKComputePipeline.h>
#include <hgl/vk/VKShaderProgram.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    /**
     * GPU 剔除系统（T3d-1b）
     * 
     * 职责：
     * - 每帧 dispatch entity_gpu_cull.glsl 计算着色器
     * - 管理剔除输出 buffer（visible_indices, indirect_commands, draw_count）
     * - 将 draw_count buffer 挂到 MaterialBatch.icb_count_buffer
     * 
     * 设计：
     * - 执行阶段：**RenderFrameSync**（T3i/2β 起：必须晚于 RenderBatch 的批装配——
     *   批标签/分区是本帧产物，cull 必须与本帧分区、本帧绘制三者同源；此前在
     *   RenderCollect 会读到上一帧的批归属 ⇒ 切片错位。钉在 RenderSceneUBOSystem
     *   之后（依赖见 DefaultSystems 注册））
     * - 输入：EntityGPU 表（世界私有）、L2W 矩阵池、GeometryAABB 池
     * - 输出：可见实体索引表、间接绘制命令表（VkDrawMeshTasksIndirectCommandEXT 格式）、可见计数
     */
    class GPUCullSystem : public System
    {
    public:
        GPUCullSystem();
        ~GPUCullSystem();

        void Update(float delta_time) override;

        // 初始化：创建 compute pipeline 和输出 buffer
        bool Initialize(ECSContext* context);

        /// **接管已强制**（A6-d T2）：opt-in 面（两开关 `SetEnabled`/`SetDrawOverrideEnabled`
        /// + `enable_gather` dispatch 参数）**整体删除** ⇒ dispatch 与 gather 接管恒开。
        /// 实际是否接管某批仍由 `PrimitiveBatchPipeline::TryApplyGPUCullOverride` 逐批 guard
        /// 判定（前提清单见该函数注释）。历史：T3f-1 形态 A 回灌随 T3g 退役（见 §8.2f）。

        /// **接管护栏的"派发已定帧"判据**（T3g）：dispatch 至少完成过 **2 帧** ⇒
        /// 实体行表 / 4-ID 表都已含就绪数据，首帧 `survivors=0` 假象不可能出现。
        /// 纯 CPU 计数（不等 GPU）。
        bool IsDispatchSettled() const { return dispatch_count_ >= 2; }


        // 获取输出 buffer（供 MaterialBatch 使用）
        graph::DeviceBuffer* GetVisibleIndicesBuffer() const { return visible_indices_buffer_; }
        graph::IndirectMeshTaskBuffer* GetIndirectCommandsBuffer() const { return indirect_commands_buffer_; }
        graph::DeviceBuffer* GetDrawCountBuffer() const { return draw_count_buffer_; }

        uint64_t GetVisibleIndicesAddress() const { return visible_indices_addr_; }
        uint64_t GetIndirectCommandsAddress() const { return indirect_commands_addr_; }
        uint64_t GetDrawCountAddress() const { return draw_count_addr_; }

        // 形态 B（T3f-2）gather 输出四表（icb 复用上面的 indirect_commands_buffer_）
        graph::DeviceBuffer* GetL2WOutBuffer() const { return l2w_out_buffer_; }
        graph::DeviceBuffer* GetCmdsOutBuffer() const { return cmds_out_buffer_; }
        graph::DeviceBuffer* GetMtlOutBuffer() const { return mtl_out_buffer_; }

        // ── T3i（③ 多批）：台架/对拍用只读视图 ────────────────────────────────
        // 本帧接管批数（0 = 无接管批 ⇒ cull 只为对拍出计数）。分区在 guard 装配、
        // 本帧消费（同帧同源）；台架在 cull 之后读，两处都在本帧内。
        /// 批数（**主帧 pass** 那份）。参考实现与绘制侧读的都是主帧口径。
        uint32_t GetBatchCount() const { return batch_count_[0]; }

        /// 本帧第 index 批的竞技场切片。越界 ⇒ false（调用方不得假定容量）。
        bool GetBatchLayout(uint32_t pass_index, uint32_t index, uint32_t &row_base, uint32_t &capacity) const
        {
            if (index >= batch_count_[pass_index] || index >= kMaxBatches || pass_index >= kArenaPasses)
                return false;

            const BatchInfoRow &row = batch_info_staging_[BatchBlockBase(pass_index) + index];
            row_base = row.row_base;
            capacity = row.capacity;
            return true;
        }

        // ── T3i（③ 多批/混材质）：批标签 / 分区 / 每批计数 ──────────────────
        // 三者均**按帧槽分份且 CPU 只写当前份**：否则本帧直写会与上一帧仍在飞行的
        // cull 竞争（同队列提交序只覆盖"新提交批次读旧批次写"，不覆盖 CPU 直写）。
        /// 批数上限。**失败模式分层**（A6-c 拍板，口径与实现一致）：
        /// · 旧：超限**逐批软降级**（该批退回 CPU 账 + 一次性 `GLogError`）——**已删**。
        /// · 今（A6-d T4b）：**fail-fast**——超限批一次性 `GLogError` 报因且**本帧不绘制**
        ///   （容量是拍板冻结值，超限 = 项目 bug，不扩容、不静默丢批）。台架 `CV_STRESS=260`
        ///   预期：`ENGAGED 256/260` + 一次报因 + 超出批无绘制。
        static constexpr uint32_t kMaxBatches      = 256;
        static constexpr uint32_t kInvalidBatchTag = 0xFFFFFFFFu;   ///< 不属任何批 ⇒ cull 跳过

        // ── 容量与按 pass 的块/扇区常量 ──────────────────────────────────────
        // ⚠ 必须声明在**成员之前**：`batch_info_staging_` 等成员数组要用它们
        //   （类内声明序 = 使用序，前向引用不成立）。
        static constexpr uint32_t kMaxVisibleEntities = 4096;

        // T4c1：竞技场按 pass 分扇区（拍板：容量扩到 2 × 4096 = 8192）。
        // 两个 pass 的绘制集不同（主帧 = GPU 剔除后子集；阴影 = 全量直通，T4c2/T4c3）
        // ⇒ 各写各的扇区，互不覆盖。**实体索引空间与批标签不在此列**：标签按实体记
        // "属哪一批"，与 pass 无关 ⇒ 仍为 kMaxVisibleEntities（一槽一份）。
        // 数值与 `RenderPassSlot::Count` 同值（保留字面量：门 `B.batch-info-struct-parity`
        // 对 GLSL 的 `ULRE_ARENA_PASSES` 做整数对拍，别名会让提取失败）。
        static constexpr uint32_t kArenaPasses        = 2;
        static_assert(kArenaPasses == RenderPassSlot::Count,
                      "kArenaPasses 与 RenderPassSlot::Count 漂移（分份遍数必须单源）");
        static constexpr uint32_t kArenaRowsPerPass   = kMaxVisibleEntities;
        static constexpr uint32_t kArenaRows          = kArenaPasses * kArenaRowsPerPass;

        /// 扇区 0 = 主帧 pass；扇区 1 = 阴影 pass。
        static constexpr uint32_t ArenaSectorBase(uint32_t pass_index)
        {
            return (pass_index < kArenaPasses ? pass_index : 0u) * kArenaRowsPerPass;
        }

        /// T4c2：批行（`BatchInfoRow`）与每批计数**按 pass 分块**，块内 = `kMaxBatches` 行 +
        /// 1 个溢出槽 ⇒ 两块共 `kArenaPasses * kBatchBlockStride` 行（块 0 = 主帧 pass、
        /// 块 1 = 阴影 pass）。块的**行空间**与竞技场扇区（`ArenaSectorBase`）是两回事：
        /// 前者是"每批的切片起点表"，后者是切片所在的行区。
        static constexpr uint32_t kBatchBlockStride = kMaxBatches + 1;
        static constexpr uint32_t BatchBlockBase(uint32_t pass_index)
        { return (pass_index < kArenaPasses ? pass_index : 0u) * kBatchBlockStride; }

        /// 批的 dispatch 档（随 `BatchInfoRow.topology` 下发；**与 `entity_gpu_cull.glsl`
        /// 的 `TOPOLOGY_*` 同步**）：决定 gather 的 X 公式（线按段数，组大小也不同）。
        static constexpr uint32_t kBatchTopologyTriangles = 0;  ///< 顶点组→三角形（VertexPassthrough）
        static constexpr uint32_t kBatchTopologyLines     = 1;  ///< 段→quad（LineQuad；每线程 1 段）

        /// 批装配期写标签：返回**当前帧槽**标签数组的 CPU 映射（进入时整表清成
        /// `kInvalidBatchTag`）。用法：`tag = BeginBatchTagWrite(pass); tag[entity_index] = b;`
        /// 同时把**本 pass 那一块**批分区清零（防上一帧残留行被本帧标签引用）。
        /// 收尾必须调 `EndBatchLayout(pass, batch_count)`（unmap 标签 + 落**整张**分区表）。
        uint32_t *BeginBatchTagWrite(uint32_t pass_index);

        /// 写一条批分区（CPU 侧暂存，`EndBatchLayout` 时落设备）
        void SetBatchLayout(uint32_t pass_index, uint32_t batch_index, uint32_t row_base, uint32_t capacity,
                            uint32_t topology, uint32_t group_size);

        /// 收尾：unmap 标签 + 把分区表（batch_count 条）写入当前帧槽 + 记 batch_count
        void EndBatchLayout(uint32_t pass_index, uint32_t batch_count);

        /// 每批计数缓冲（含末位 overflow 槽）：绘制侧把它当 `icb_count_buffer` 用
        graph::DeviceBuffer *GetBatchCounterBuffer() const { return batch_counter_buffer_; }
        /// 批 b 的计数在缓冲内的字节偏移（当前帧槽 × pass 块 + 批号）；绘制侧取
        /// `MaterialBatch::gpu_slice[pass].count_buffer_offset`（T4c3c 起切片按 pass 存）
        uint32_t GetBatchCounterByteOffset(uint32_t pass_index, uint32_t batch_index) const;
        /// overflow 计数（末位槽）的字节偏移
        uint32_t GetOverflowCounterByteOffset(uint32_t pass_index) const;

        /// 台架读回：映射当前帧槽的计数数组（**调用前须 SyncForParityReadback**）。
        /// 返回指针含 `kMaxBatches + 1` 项，末项 = overflow 计数。
        uint32_t *MapCurrentBatchCounters(uint32_t pass_index);
        void      UnmapCurrentBatchCounters();

        /// 读回可见计数（**仅限对拍台架**，T3g）：生产路径零 CPU 同步 ⇒ 本函数
        /// **不等 GPU**，读到的可能是上一已定帧的值。调用方（CPUCullReferenceSystem）
        /// 必须先 `SyncForParityReadback()` 把队列排到本帧 dispatch 完成，否则读数
        /// 与真值有至多一帧漂移——对"每 60 帧一次"的台架采样无碍，但绝不可作为
        /// 生产判据（形态 A 的回灌就是死在这里被退役的，§8.2f）。
        uint32_t GetGPUVisibleCount() const;

        /// **对拍台架专用同步点**（T3g）：等本系统队列最近一次提交完成（fence）。
        /// 只在台架采样帧调用 ⇒ 稳态生产路径 0 同步开销（`PBR_PERF` 测量运行根本
        /// 不调台架）。非采样帧调它 = 白白 stall ⇒ 别在每帧路径上调。
        bool SyncForParityReadback();

        /// **CPU→GPU 交接沿专用排空**（T3g）：guard 首次接管的那一帧要 `delete`
        /// 批自有 ICB/行表，而它们可能仍被**上一帧主帧 cmd** 引用——自家 fence
        /// 只覆盖自家提交（同队列各批"按序开始、可乱序完成"），等它不够 ⇒
        /// 唯一稳妥 = `vkQueueWaitIdle` 排空。每次交接沿付一次，稳态不付。
        /// （VUID-vkFreeMemory-memory-00677 实测踩中：RPBS_IndirectMeshTaskBuffer
        /// in use by Swapchain_RenderCmdBuffer_0。）
        void DrainForHandover();

        /// **SSBO DUMP 诊断**：同步读回并向日志 Dump 所有帧槽的计数缓冲、布局缓冲、标签分布与全局幸存数
        void DumpCullSSBOs(const char *stage_tag = "CULL_DUMP");

        /// **形态 B（T3f-2）接管 hook**：本帧 build 之前由 `PrimitiveBatchPipeline::
        /// RunBatching()` 调用（批对象已被 `MaterialBatch::Clear()` 复位成"自有 buffer +
        /// override=false"状态）。前提校验全部通过 ⇒ 释放批自有行表、四张 gather 表 +
        /// icb + count 指向剔除输出、置 `gpu_driven_override`（既有契约：跳 sort/ensure/
        /// write、每项自成 DrawBatch 给 maxDrawCount 记账）。
        /// T3i：hook 逐批调用（`TryApplyGPUCullOverride` 遍历含几何批），未接管的批
        /// 标签留无效 ⇒ cull 不为其写行、CPU 路径照旧绘制（**按批粒度**）。
        /// 前提（§8.3/§8.5，一条不满足 ⇒ **该批**不接管 + 一次性报错，绝不让批指向错数据）：
        /// opt-in 开 && 主帧 pass && 批内全
        /// PrimitiveRenderItem && 全 mesh 批 && 批容量 ≤ gather 容量
        /// && 全 item 有 owner 实体（禁 VDM 等无实体项——4-ID 表只覆盖实体）&&
        /// 每实体池行存在 && 非 lines &&
        /// 阴影全默认（CanReceiveShadow && bias==1.0 ⇒ gather 写 0 与 CPU 行逐字节一致）。
        /// T3i：逐批调用（`batch_index` = 该批在**本帧分区表**中的槽号，由 guard 装配时
        /// 分配并写入标签/分区；`row_base`/`capacity` 从分区表取，口径唯一）。
        /// @return true = 本帧该批由 GPU gather 输出接管绘制。
        /// T4c3c：`pass_index` 决定切片落**哪个块/扇区**（`BatchBlockBase(pass)`），
        /// 以及写进批的**哪一份**切片（`MaterialBatch::gpu_slice[pass]`）。
        bool ApplyGPUCullOverrideHook(MaterialBatch *batch, uint32_t batch_index, uint32_t pass_index);

        // 2026-10-10（审计 §1.1）：`IsOverrideActiveThisFrame()` 已删除 —— 全仓零调用者，且其自注
        //「collect 的 CPU 剔除跳不跳全看它」与实际闸门不符（会误导读者去找错地方）。

        /// 延迟初始化（pipeline/输出缓冲/派发资源）：由**首次 Update 或首次批装配 guard**
        /// 触发（两者都在 render pass 内、设备已绑）。⚠ guard 相位（RenderBatch）早于
        /// Update 相位（RenderFrameSync）⇒ 新世界首帧必须由 guard 侧确保资源就位，
        /// 否则 BeginBatchTagWrite 拿到空缓冲、布局永不落、该世界永不画（RTT 实测）。
        bool EnsureInitialized();

    private:
        // 创建 compute pipeline
        bool CreateComputePipeline();

        // 创建输出 buffer
        bool CreateOutputBuffers();

        /// 一次性创建派发资源（T3g）：队列（fence 环 = 帧槽数）+ 命令缓冲环。
        /// 泄漏修复的本体：这些对象**只在这里 new 一次**，析构显式释放。
        bool CreateDispatchResources();

        // 每帧 dispatch
        void DispatchCullShader();

    private:
        ECSContext* context_ = nullptr;
        graph::VulkanDevice* device_ = nullptr;
        bool initialized_ = false;

        // Compute pipeline
        graph::ShaderProgram* shader_program_ = nullptr;
        graph::ComputePipeline* compute_pipeline_ = nullptr;

        // 输出 buffer
        graph::DeviceBuffer *visible_indices_buffer_ = nullptr;
        graph::IndirectMeshTaskBuffer *indirect_commands_buffer_ = nullptr;  // 改为 IndirectMeshTaskBuffer
        graph::DeviceBuffer *draw_count_buffer_ = nullptr;

        // ── T3i：批标签 / 分区 / 计数（按帧槽分份；CPU 只写当前份）──
        graph::DeviceBuffer *batch_tag_buffer_ = nullptr;      // kSlots × 4096 × u32
        graph::DeviceBuffer *batch_info_buffer_ = nullptr;     // kSlots × kMaxBatches × 8 B
        graph::DeviceBuffer *batch_counter_buffer_ = nullptr;  // kSlots × (kMaxBatches+1) × u32
        uint64_t batch_tag_addr_ = 0;
        uint64_t batch_info_addr_ = 0;
        uint64_t batch_counter_addr_ = 0;

        /// 批信息行（**与 `entity_gpu_cull.glsl` 的 `BatchInfoRow` 逐字段同序同型**；
        /// 16 B/行）。后两字段 = **dispatch 档**：gather 的 X 公式按它选（线按段数、组大小不同）。
        struct BatchInfoRow
        {
            uint32_t row_base;    // 本批切片在共享竞技场里的起始行
            uint32_t capacity;    // 切片容量（= 本批 item 数；绘制侧 maxDrawCount）
            uint32_t topology;    // kBatchTopology*（与 GLSL 的 TOPOLOGY_* 同步）
            uint32_t group_size;  // MeshDispatchGroupSize(is_lines)（CPU 单源下发，GLSL 不硬编码）
        };

        /// CPU 暂存：两块（每块 kBatchBlockStride 行）。未写的那块保持零 ⇒ 该 pass 的
        /// `capacity == 0` ⇒ shader 跳过该 pass（T4c3 起阴影 pass 才会写自己的块）。
        BatchInfoRow batch_info_staging_[kArenaPasses * kBatchBlockStride] = {};
        /// 本帧批数，**按 pass 分开**（0 = 无批）。此前是单份共享值 ⇒ 一帧内后一次
        /// `EndBatchLayout` 的批数会作用到两个 pass（实测：主帧派发拿到阴影的批数 ⇒
        /// 主帧批 1 被整批丢弃）。着色器 `pc.batch_count` 取主帧那份。
        uint32_t batch_count_[kArenaPasses] = {0, 0};
        uint32_t *batch_tag_cpu_ = nullptr;           ///< 当前帧槽标签数组的映射
        uint32_t  batch_tag_cpu_slot_ = UINT32_MAX;   ///< 映射对应的帧槽（换槽重映射）
        uint32_t *batch_counter_cpu_ = nullptr;       ///< 台架读回时的映射
        uint32_t counters_sync_frame_ = 0xFFFFFFFFu;  ///< §9 M6：最近一次 SyncForParityReadback 的帧号（Map 前校验）

        // 形态 B（T3f-2）gather 输出（容量 = kMaxVisibleEntities；被接管批直接指向它们）
        graph::DeviceBuffer *l2w_out_buffer_ = nullptr;
        graph::DeviceBuffer *cmds_out_buffer_ = nullptr;
        graph::DeviceBuffer *mtl_out_buffer_ = nullptr;

        /// guard 记账（ApplyGPUCullOverrideHook 每帧 build 时评估；collect 的
        /// "CPU 剔除跳不跳" 读本帧结果 ⇒ 两本账恒二选一）：
        /// `override_eval_frame_` = 最近一次评估帧号，`override_active_frame_` =
        /// 最近一次**通过**的帧号；两者相等 ⇔ 本帧正在接管。
        uint64_t override_eval_frame_ = 0;
        uint64_t override_active_frame_ = 0;

        /// 单批前提的一次性报错门（前提不满足 = 场景装配与管线口径冲突，
        /// fail-fast 报一次，不静默退回 CPU 账假装一切正常）
        bool override_reject_reported_ = false;

        // ── 派发资源（T3g：生命周期一次化，修每帧 new 队列/命令缓冲的泄漏）──
        // 队列一份：fence 环深度 = 帧槽数（DeviceQueue::Submit 自动轮转 fence）；
        // 与主帧共用 attr->graphics_queue ⇒ 同队列 FIFO 天然保证
        // [本帧 cull cmd → 主帧 draw] 的消费序（去 WaitLastSubmitFence 的依据）。
        graph::DeviceQueue *cull_queue_ = nullptr;
        // 命令缓冲环：槽号 = frame_index % HGL_FRAME_SLOT_TOTAL。重录前等待该 cmd
        // 上次提交对应的 fence 槽；帧槽与 fence 槽独立记账，避免跳过派发时错配。
        graph::ComputeCmdBuffer *cull_cmds_[HGL_FRAME_SLOT_TOTAL] = {};
        uint32_t cull_cmd_fence_slots_[HGL_FRAME_SLOT_TOTAL] = {};
        bool cull_cmd_submitted_[HGL_FRAME_SLOT_TOTAL] = {};
        uint32_t dispatch_count_ = 0;   ///< 成功派发帧数（IsDispatchSettled 判据）

        /// 批 1.3 C5：一次性告警闩改**每实例**（每个世界一个 GPUCullSystem 实例 ⇒ 实例级 = 世界级）。
        /// 原 `static bool` 是**进程级** ⇒ 第一个世界报过之后，第二个世界（如离屏世界）
        /// 的同类配置错误被永久静默，排查时看不见。
        bool reported_misaligned_rows_ = false;
        bool reported_unsynced_map_    = false;

        /// 各帧槽的批布局**是否已被写过**（T4c-ini-2）。
        /// 用途：布局只在"轮到该槽的那一帧"写，而剔除派发在**同一帧**读它 ⇒ 首次写入的那一帧，
        /// 派发必然看到初值（`capacity == 0`）⇒ 那一帧若仍让批接管，主帧 count 就是 0（物体不画）。
        /// 所以：接管前查"该槽此前是否已写过"，未写过 ⇒ 本帧只写布局、不接管（回落 CPU 账）。

        // 设备地址（BDA）
        uint64_t visible_indices_addr_ = 0;
        uint64_t indirect_commands_addr_ = 0;
        uint64_t draw_count_addr_ = 0;
        uint64_t l2w_out_addr_ = 0;    // 形态 B gather 三表地址（push constant 下发用）
        uint64_t cmds_out_addr_ = 0;
        uint64_t mtl_out_addr_ = 0;

        static constexpr uint32_t kWorkGroupSize = 64;
    };
}
