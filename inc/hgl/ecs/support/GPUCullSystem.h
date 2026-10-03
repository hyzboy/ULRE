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

        /// **第三个 opt-in（默认关，T3f-2 形态 B 预留）**：`GPUCullResultHookSystem`
        /// 直连接管批次间接绘制（`gpu_driven_override` + ICB/count 替换）。
        /// 开启前必须先解决：per-batch 行表与剔除输出**同序**（atomicAdd 序不定）。
        void SetDrawOverrideEnabled(bool enabled);
        bool IsDrawOverrideEnabled() const { return override_draw_; }

        // 获取输出 buffer（供 MaterialBatch 使用）
        graph::DeviceBuffer* GetVisibleIndicesBuffer() const { return visible_indices_buffer_; }
        graph::IndirectMeshTaskBuffer* GetIndirectCommandsBuffer() const { return indirect_commands_buffer_; }
        graph::DeviceBuffer* GetDrawCountBuffer() const { return draw_count_buffer_; }

        uint64_t GetVisibleIndicesAddress() const { return visible_indices_addr_; }
        uint64_t GetIndirectCommandsAddress() const { return indirect_commands_addr_; }
        uint64_t GetDrawCountAddress() const { return draw_count_addr_; }

        // 获取 GPU 剔除的可见实体数量（从 draw_count buffer 读取）
        uint32_t GetGPUVisibleCount() const;

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
        graph::DeviceBuffer* visible_indices_buffer_ = nullptr;
        graph::IndirectMeshTaskBuffer* indirect_commands_buffer_ = nullptr;  // 改为 IndirectMeshTaskBuffer
        graph::DeviceBuffer* draw_count_buffer_ = nullptr;

        // 设备地址（BDA）
        uint64_t visible_indices_addr_ = 0;
        uint64_t indirect_commands_addr_ = 0;
        uint64_t draw_count_addr_ = 0;

        // 容量
        static constexpr uint32_t kMaxVisibleEntities = 4096;
        static constexpr uint32_t kWorkGroupSize = 64;
    };
}
