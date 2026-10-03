#pragma once

#include <hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class GPUCullSystem;

    /**
     * GPUCullResultHookSystem - 将 GPU 剔除结果挂到 MaterialBatch
     * 
     * 职责：
     * - 在 RenderFrameSync 阶段执行
     * - 从 GPUCullSystem 获取剔除结果 buffer
     * - 将 indirect_commands_buffer 挂到 MaterialBatch.icb_mesh_tasks
     * - 将 draw_count_buffer 挂到 MaterialBatch.icb_count_buffer
     * - 设置 batch->gpu_driven_override = true
     * 
     * 这样 MaterialBatch 就会使用 GPU 剔除的结果进行间接绘制
     */
    class GPUCullResultHookSystem : public System
    {
    public:
        GPUCullResultHookSystem();
        ~GPUCullResultHookSystem() override = default;

        void Update(float deltaTime) override;

        void SetContext(ECSContext* ctx) { context = ctx; }

    private:
        GPUCullSystem* GetGPUCullSystem() const;
    };
}
