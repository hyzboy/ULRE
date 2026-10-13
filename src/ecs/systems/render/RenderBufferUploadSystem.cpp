#include<hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VK.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/IGPUBuffer.h>
#include<cstdio>

namespace hgl::ecs
{
    RenderBufferUploadSystem::RenderBufferUploadSystem(const std::string& name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderBufferUpload);
    }

    void RenderBufferUploadSystem::Update(float /*deltaTime*/)
    {
        ECSContext *ctx = world ? world : context;
        if (!ctx)
        {
            GLogInfo("[RenderBufferUpload] skip: no ECS context");
            return;
        }

        graph::RenderCmdBuffer *cmdBuffer = ctx->GetCurrentRenderCmd();
        if (!cmdBuffer)
        {
            GLogInfo("[RenderBufferUpload] skip: no current render command buffer");
            return;
        }

        graph::VulkanDevice* active_device = device;
        if (!active_device)
            active_device = ctx->GetGPUDevice();

        if (!active_device)
        {
            GLogInfo("[RenderBufferUpload] skip: no active GPU device");
            return;
        }

        const auto &registry = active_device->GetGPUBufferRegistry();
        if (registry.empty())
        {
            // 空 registry 是常态：设备上没有存活的 StagedBuffer（当前上传
            // 路径多为 host-visible 直写 + 上方 SyncToGPU），静默跳过即可，
            // 不打日志（曾每帧两条刷屏）。首个 StagedBuffer 创建后本循环
            // 自然开始工作。
            return;
        }

        const VkCommandBuffer vk_cmd = static_cast<VkCommandBuffer>(*cmdBuffer);

        // Flush all dirty IGPUBuffer objects
        bool any_uploads = false;

        for (auto *buf : registry)
        {
            if (buf && buf->IsDirty())
            {
                buf->CopyToDevice(vk_cmd);
                // CopyToDevice calls ClearDirty internally for StagedBuffer
                any_uploads = true;
            }
        }

        // Only emit the transfer→vertex barrier when transfers actually happened.
        // Skipping when any_uploads==false prevents an invalid
        // VK_PIPELINE_STAGE_TRANSFER_BIT barrier inside a Vulkan render pass
        // on the second call (RenderGraph re-runs Update inside BeginRenderPass).
        if (!any_uploads)
        {
            //GLogInfo("[RenderBufferUpload] no dirty buffers; skip transfer barrier");
            //std::fprintf(stderr, "[RenderBufferUpload] no dirty buffers; skip transfer barrier\n");
            return;
        }

        cmdBuffer->MemoryBarrier2(
            VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT
          | VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT
          | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT
          | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT
          | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
          | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT
          | VK_ACCESS_2_INDEX_READ_BIT
          | VK_ACCESS_2_UNIFORM_READ_BIT
          | VK_ACCESS_2_SHADER_READ_BIT
          | VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
    }
}//namespace hgl::ecs
