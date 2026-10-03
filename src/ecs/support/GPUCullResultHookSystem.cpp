#include <hgl/ecs/support/GPUCullResultHookSystem.h>
#include <hgl/ecs/support/GPUCullSystem.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/MaterialBatch.h>

namespace hgl::ecs
{
    GPUCullResultHookSystem::GPUCullResultHookSystem()
        : System("GPUCullResultHookSystem")
    {
        SetExecutionPhase(ExecutionPhase::RenderFrameSync);
    }

    GPUCullSystem* GPUCullResultHookSystem::GetGPUCullSystem() const
    {
        if (!context)
            return nullptr;

        return context->GetSystem<GPUCullSystem>().get();
    }

    void GPUCullResultHookSystem::Update(float deltaTime)
    {
        if (!context)
            return;

        GPUCullSystem* gpu_cull = GetGPUCullSystem();
        if (!gpu_cull)
            return;

        // opt-in 门（默认关）：未显式允许**接管绘制** ⇒ 完全不碰批次。
        // 本 hook 会把被接管批次的 CPU 侧 ICB/行表生成整段跳过（gpu_driven_override），
        // 无条件接管 = 所有示例的间接计数被换成一个还没跑通的 count buffer ⇒
        // 画面全空（RenderDoc 实测 vkCmdDrawMeshTasksIndirectCountEXT drawCount=0）。
        // 与 dispatch 开关分离：接管还要求 per-batch 行表与剔除输出同序（T3f）。
        if (!gpu_cull->IsDrawOverrideEnabled())
            return;

        // 获取 GPU 剔除结果的 buffer
        auto* indirect_commands_buffer = gpu_cull->GetIndirectCommandsBuffer();
        auto* draw_count_buffer = gpu_cull->GetDrawCountBuffer();

        if (!indirect_commands_buffer || !draw_count_buffer)
            return;

        // 遍历所有 MaterialBatch，将 GPU 剔除结果挂到第一个有内容的 batch
        auto& cache = context->GetRenderFrameCache();
        bool hooked = false;
        
        for (auto& pair : cache.materialBatches)
        {
            MaterialBatch* batch = pair.second.get();
            if (!batch || batch->items.empty())
                continue;

            if (!hooked)
            {
                // 第一个有内容的 batch：挂载 GPU 剔除结果
                batch->gpu_driven_override = true;
                batch->own_icb_mesh_tasks = false;  // 不拥有，由 GPUCullSystem 管理
                batch->own_mesh_draw_params = false;
                batch->own_l2w_index = false;
                batch->own_material_data_rows = false;

                // 挂载间接命令 buffer（VkDrawMeshTasksIndirectCommandEXT 格式）
                batch->icb_mesh_tasks = indirect_commands_buffer;

                // 挂载计数 buffer
                batch->icb_count_buffer = draw_count_buffer;
                batch->icb_count_buffer_offset = 0;
                
                hooked = true;
            }
            else
            {
                // 其他 batch：清空 items，不绘制
                batch->items.clear();
                batch->draw_batches_count = 0;
            }
        }
    }
}
