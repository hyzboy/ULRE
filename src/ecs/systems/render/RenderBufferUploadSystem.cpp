#include<hgl/ecs/systems/render/RenderBufferUploadSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/RenderItemDataStorage.h>
#include<hgl/ecs/support/DrawItemIDStorage.h>
#include<hgl/vk/VK.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/IGPUBuffer.h>
#include<string>
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

        // Sync Level-1 GlobalRenderItemBuffer (4-ID descriptor storage SSBO)
        if (auto *render_item_storage = ctx->GetRenderItemStorage())
        {
            render_item_storage->SyncToGPU(active_device);
        }

        // Sync Level-2 DrawItemIDBuffer (secondary index SSBO)
        if (auto *draw_item_id_storage = ctx->GetDrawItemIDStorage())
        {
            draw_item_id_storage->SyncToGPU(active_device);
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
        uint32_t scanned_count = 0;
        uint32_t dirty_count = 0;
        uint32_t transform_tagged_count = 0;
        uint32_t transform_tagged_dirty_count = 0;

        for (auto *buf : registry)
        {
            ++scanned_count;

            std::string buf_name;
            if (buf)
                buf_name = buf->GetBufferName();

            const bool is_transform_related =
                   (buf_name.find("LocalToWorld") != std::string::npos)
                || (buf_name.find("TransformID") != std::string::npos)
                || (buf_name.find("L2W") != std::string::npos);

            if (is_transform_related)
                ++transform_tagged_count;

            if (buf && buf->IsDirty())
            {
                ++dirty_count;
                if (is_transform_related)
                    ++transform_tagged_dirty_count;

                //GLogInfo("[RenderBufferUpload] CopyToDevice: %s (size=%llu)",
                //          buf->GetBufferName().empty() ? "(unnamed)" : buf->GetBufferName().c_str(),
                //          static_cast<unsigned long long>(buf->GetSize()));
                //std::fprintf(stderr,
                //             "[RenderBufferUpload] CopyToDevice: %s (size=%llu)\n",
                //             buf->GetBufferName().empty() ? "(unnamed)" : buf->GetBufferName().c_str(),
                //             static_cast<unsigned long long>(buf->GetSize()));
                buf->CopyToDevice(vk_cmd);
                // CopyToDevice calls ClearDirty internally for StagedBuffer
                any_uploads = true;
            }
        }

        //GLogInfo("[RenderBufferUpload] scan summary: scanned=%u dirty=%u transform_tagged=%u transform_tagged_dirty=%u",
        //          scanned_count,
        //          dirty_count,
        //          transform_tagged_count,
        //          transform_tagged_dirty_count);
        //std::fprintf(stderr,
        //         "[RenderBufferUpload] scan summary: scanned=%u dirty=%u transform_tagged=%u transform_tagged_dirty=%u\n",
        //         scanned_count,
        //         dirty_count,
        //         transform_tagged_count,
        //         transform_tagged_dirty_count);

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
