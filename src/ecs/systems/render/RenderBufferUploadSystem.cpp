#include<hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VK.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/IGPUBuffer.h>

namespace hgl::ecs
{
    RenderBufferUploadSystem::RenderBufferUploadSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::RenderSubmit);
        SetExecutionOrder(ExecutionPhase::RenderBufferUpload);
    }

    void RenderBufferUploadSystem::Update(float /*deltaTime*/)
    {
        ECSContext *ctx = world ? world : context;
        if (!ctx)
            return;

        graph::RenderCmdBuffer *cmdBuffer = ctx->GetCurrentRenderCmd();
        if (!cmdBuffer)
            return;

        graph::VulkanDevice* active_device = device;
        if (!active_device)
            active_device = ctx->GetGPUDevice();

        if (!active_device)
            return;

        const auto &registry = active_device->GetGPUBufferRegistry();
        if (registry.empty())
            return;

        const VkCommandBuffer vk_cmd = static_cast<VkCommandBuffer>(*cmdBuffer);

        // Flush all dirty IGPUBuffer objects
        for (auto *buf : registry)
        {
            if (buf && buf->IsDirty())
            {
                buf->CopyToDevice(vk_cmd);
                // CopyToDevice calls ClearDirty internally for StagedBuffer
            }
        }

        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
                              | VK_ACCESS_INDEX_READ_BIT
                              | VK_ACCESS_UNIFORM_READ_BIT
                              | VK_ACCESS_SHADER_READ_BIT
                              | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;

        vkCmdPipelineBarrier(vk_cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
                           | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
                           | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
                           | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                           | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0, 1, &barrier, 0, nullptr, 0, nullptr);
    }
}//namespace hgl::ecs