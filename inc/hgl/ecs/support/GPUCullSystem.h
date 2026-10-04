#pragma once
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
     * - 执行阶段：RenderCollect（在 RunCulling 之后）
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

        /// **opt-in 开关（默认关）**：dispatch 剔除着色器 + 供 CPU 对拍读取。
        /// 本管线仍在 T3f 验收阶段，未逐示例验证过像素正确性 ⇒ 不得默认全局
        /// 接管批次（否则 `count=0` 会把所有示例画空）。
        /// 关 ⇒ `dispatch_count_` 归零：重新开启从"未定帧"状态起算（T3g 防坑 3：
        /// 场景切换/暂停再开不能让旧计数放行误接管）。
        void SetEnabled(bool enabled);
        bool IsEnabled() const { return enabled_; }

        /// **第二个 opt-in（默认关，T3f-2 形态 B）**：允许 gather 输出接管绘制。
        /// 实际接管由 `PrimitiveBatchPipeline::TryApplyGPUCullOverride` 逐帧 guard 判定
        /// （前提清单见该函数注释）；本开关只是总闸 + `enable_gather` dispatch 参数。
        /// （原 `GPUCullResultHookSystem` 的跨帧接线方式与批生命周期不匹配，已删除。）
        /// （T3f-1 的形态 A `SetApplyVisibleSetEnabled` 已整体退役：回灌 = 每帧
        /// CPU Map 读回，生产路径同步点；对拍台架已覆盖其验证价值，见 §8.2f。）
        void SetDrawOverrideEnabled(bool enabled);
        bool IsDrawOverrideEnabled() const { return override_draw_; }

        /// **接管护栏的"派发已定帧"判据**（T3g）：dispatch 至少完成过 **2 帧** ⇒
        /// 实体行表 / 4-ID 表都已含就绪数据，首帧 `survivors=0` 假象不可能出现。
        /// 纯 CPU 计数（不等 GPU）；`SetEnabled(false)` 归零 ⇒ 重开必须重新满 2 帧。
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

        /// gather 输出表容量（行数/元素数 = kMaxVisibleEntities）：guard 校验批容量用
        static constexpr uint32_t GetGatherCapacity() { return kMaxVisibleEntities; }

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

        /// **形态 B（T3f-2）接管 hook**：本帧 build 之前由 `PrimitiveBatchPipeline::
        /// RunBatching()` 调用（批对象已被 `MaterialBatch::Clear()` 复位成"自有 buffer +
        /// override=false"状态）。前提校验全部通过 ⇒ 释放批自有行表、四张 gather 表 +
        /// icb + count 指向剔除输出、置 `gpu_driven_override`（既有契约：跳 sort/ensure/
        /// write、每项自成 DrawBatch 给 maxDrawCount 记账）。
        /// 前提（§8.3，一条不满足 ⇒ 本帧不接管 + 一次性报错，绝不让批指向错数据）：
        /// opt-in 开 && 主帧 pass && **恰好一个含几何批**（R4 单批口径）&& 批内全
        /// PrimitiveRenderItem（禁实例化/多实例）&& 全 mesh 批 && 批容量 ≤ gather 容量
        /// && 全 item 有 owner 实体（禁 VDM 等无实体项——4-ID 表只覆盖实体）&&
        /// 每实体池行存在 && 非 meshlet（池行 addr_meshlets==0）&& 非 lines &&
        /// 阴影全默认（CanReceiveShadow && bias==1.0 ⇒ gather 写 0 与 CPU 行逐字节一致）。
        /// @return true = 本帧该批由 GPU gather 输出接管绘制。
        bool ApplyGPUCullOverrideHook(MaterialBatch *batch);

        /// guard 的**本帧有效**判定：collect 的 CPU 剔除跳不跳全看它（接管与 CPU 账
        /// 必须按同一帧的 guard 结果二选一，否则 = 双份账/漏帧）。
        bool IsOverrideActiveThisFrame() const { return override_active_frame_ == override_eval_frame_; }

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

        // 延迟初始化（在第一次 Update 时执行）
        bool EnsureInitialized();

    private:
        ECSContext* context_ = nullptr;
        graph::VulkanDevice* device_ = nullptr;
        bool initialized_ = false;
        bool enabled_ = false;        ///< opt-in 开关：默认关（dispatch + 对拍）
        bool override_draw_ = false;  ///< opt-in 开关：默认关（接管批次间接绘制）

        // Compute pipeline
        graph::ShaderProgram* shader_program_ = nullptr;
        graph::ComputePipeline* compute_pipeline_ = nullptr;

        // 输出 buffer
        graph::DeviceBuffer *visible_indices_buffer_ = nullptr;
        graph::IndirectMeshTaskBuffer *indirect_commands_buffer_ = nullptr;  // 改为 IndirectMeshTaskBuffer
        graph::DeviceBuffer *draw_count_buffer_ = nullptr;

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
        // 命令缓冲环：槽号 = frame_index % HGL_FRAME_SLOT_TOTAL。同槽再次重录
        // 隔 8 帧、在飞深度 ≤4 ⇒ 必已执行完（与 swapchain per-image cmd 同一安全
        // 假设，见 SwapchainRenderTarget::BeginRender；pool 带
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT，Begin 合法）。
        graph::ComputeCmdBuffer *cull_cmds_[HGL_FRAME_SLOT_TOTAL] = {};
        uint32_t dispatch_count_ = 0;   ///< 成功派发帧数（IsDispatchSettled 判据）

        // 设备地址（BDA）
        uint64_t visible_indices_addr_ = 0;
        uint64_t indirect_commands_addr_ = 0;
        uint64_t draw_count_addr_ = 0;
        uint64_t l2w_out_addr_ = 0;    // 形态 B gather 三表地址（push constant 下发用）
        uint64_t cmds_out_addr_ = 0;
        uint64_t mtl_out_addr_ = 0;

        // 容量
        static constexpr uint32_t kMaxVisibleEntities = 4096;
        static constexpr uint32_t kWorkGroupSize = 64;
    };
}
