#include<hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/vk/VK.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/IGPUBuffer.h>
#include<string>
#include<cstdio>

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
            GLogInfo("[RenderBufferUpload] skip: GPU buffer registry is empty");
            return;
        }

        const VkCommandBuffer vk_cmd = static_cast<VkCommandBuffer>(*cmdBuffer);

        // Flush all dirty IGPUBuffer objects
        bool any_uploads = false;
        uint32_t scanned_count = 0;
        uint32_t dirty_count = 0;

        for (auto *buf : registry)
        {
            ++scanned_count;

            if (buf && buf->IsDirty())
            {
                ++dirty_count;

                GLogInfo("[RenderBufferUpload] CopyToDevice: %s (size=%llu)",
                          buf->GetBufferName().empty() ? "(unnamed)" : buf->GetBufferName().c_str(),
                          static_cast<unsigned long long>(buf->GetSize()));
                std::fprintf(stderr,
                             "[RenderBufferUpload] CopyToDevice: %s (size=%llu)\n",
                             buf->GetBufferName().empty() ? "(unnamed)" : buf->GetBufferName().c_str(),
                             static_cast<unsigned long long>(buf->GetSize()));
                buf->CopyToDevice(vk_cmd);
                // CopyToDevice calls ClearDirty internally for StagedBuffer
                any_uploads = true;
            }
        }

        GLogInfo("[RenderBufferUpload] scan summary: scanned=%u dirty=%u",
              scanned_count,
              dirty_count);
        std::fprintf(stderr,
             "[RenderBufferUpload] scan summary: scanned=%u dirty=%u\n",
                 scanned_count,
             dirty_count);

        // Only emit the transfer→vertex barrier when transfers actually happened.
        // Skipping when any_uploads==false prevents an invalid
        // VK_PIPELINE_STAGE_TRANSFER_BIT barrier inside a Vulkan render pass
        // on the second call (RenderGraph re-runs Update inside BeginRenderPass).
        if (!any_uploads)
        {
            GLogInfo("[RenderBufferUpload] no dirty buffers; skip transfer barrier");
            std::fprintf(stderr, "[RenderBufferUpload] no dirty buffers; skip transfer barrier\n");
            return;
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