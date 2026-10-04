#pragma once
#include <hgl/ecs/core/System.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/vk/buffer/IndirectCommandBuffer.h>
#include <hgl/vk/pipeline/VKComputePipeline.h>
#include <hgl/vk/VKShaderProgram.h>
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
        void SetEnabled(bool enabled);
        bool IsEnabled() const { return enabled_; }

        /// **第二个 opt-in（默认关，T3f-1 形态 A）**：把 GPU 幸存集**回灌**
        /// `RenderItem::isVisible`（dispatch 后读回 visible_indices，按实体 index
        /// 对齐逐项覆盖）。下游 batch 构建 / 行表 / ICB **全部走既有 CPU 路径**
        /// ⇒ 无新数据契约、无 GPU 序与 CPU 行表的对齐问题。
        /// 接线前提（已核实的执行序）：本系统在 `RenderCollect` 内排在
        /// `PrimitiveCullSystem`（CPU 剔除写 isVisible）**之后**、`PrimitiveBuildSystem`
        /// （`RenderBatch` 消费 isVisible）之前 ⇒ 回灌的是本帧最终判据。
        /// CPU 的 `PerformFrustumCulling` 仍跑：它写的 isVisible 被本回灌覆盖，
        /// 但 CPU 参考对拍（CPUCullReferenceSystem）口径不变。
        void SetApplyVisibleSetEnabled(bool enabled);
        bool IsApplyVisibleSetEnabled() const { return apply_visible_set_; }

        /// **第三个 opt-in（默认关，T3f-2 形态 B）**：允许 gather 输出接管绘制。
        /// 实际接管由 `PrimitiveBatchPipeline::TryApplyGPUCullOverride` 逐帧 guard 判定
        /// （前提清单见该函数注释）；本开关只是总闸 + `enable_gather` dispatch 参数。
        /// （原 `GPUCullResultHookSystem` 的跨帧接线方式与批生命周期不匹配，已删除。）
        void SetDrawOverrideEnabled(bool enabled);
        bool IsDrawOverrideEnabled() const { return override_draw_; }

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

        // 获取 GPU 剔除的可见实体数量（从 draw_count buffer 读取）
        uint32_t GetGPUVisibleCount() const;

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

        // 每帧 dispatch
        void DispatchCullShader();

        /// 形态 A 回灌（T3f-1）：GPU 幸存实体 index 集覆盖 `RenderItem::isVisible`
        void ApplyVisibleSetToRenderItems(uint32_t visible_count);

        // 延迟初始化（在第一次 Update 时执行）
        bool EnsureInitialized();

    private:
        ECSContext* context_ = nullptr;
        graph::VulkanDevice* device_ = nullptr;
        bool initialized_ = false;
        bool enabled_ = false;        ///< opt-in 开关：默认关（dispatch + 对拍）
        bool override_draw_ = false;  ///< opt-in 开关：默认关（接管批次间接绘制）
        bool apply_visible_set_ = false; ///< opt-in 开关：默认关（形态 A：回灌 isVisible）

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
