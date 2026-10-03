#pragma once
#include <hgl/ecs/core/System.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
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
     * - 输出：可见实体索引表、间接绘制命令表、可见计数
     */
    class GPUCullSystem : public System
    {
    public:
        GPUCullSystem();
        ~GPUCullSystem();

        void Update(float delta_time) override;

        // 初始化：创建 compute pipeline 和输出 buffer
        bool Initialize(ECSContext* context);

        // 获取输出 buffer 地址（供 MaterialBatch 使用）
        graph::DeviceBuffer* GetVisibleIndicesBuffer() const { return visible_indices_buffer_; }
        graph::DeviceBuffer* GetIndirectCommandsBuffer() const { return indirect_commands_buffer_; }
        graph::DeviceBuffer* GetDrawCountBuffer() const { return draw_count_buffer_; }

        uint64_t GetVisibleIndicesAddress() const { return visible_indices_addr_; }
        uint64_t GetIndirectCommandsAddress() const { return indirect_commands_addr_; }
        uint64_t GetDrawCountAddress() const { return draw_count_addr_; }

    private:
        // 创建 compute pipeline
        bool CreateComputePipeline();

        // 创建输出 buffer
        bool CreateOutputBuffers();

        // 每帧 dispatch
        void DispatchCullShader();

    private:
        ECSContext* context_ = nullptr;
        graph::VulkanDevice* device_ = nullptr;

        // Compute pipeline
        graph::ShaderProgram* shader_program_ = nullptr;
        graph::ComputePipeline* compute_pipeline_ = nullptr;

        // 输出 buffer
        graph::DeviceBuffer* visible_indices_buffer_ = nullptr;
        graph::DeviceBuffer* indirect_commands_buffer_ = nullptr;
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
