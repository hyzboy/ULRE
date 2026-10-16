#include<hgl/vk/VKBufferReadback.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/vk/buffer/BufferMemory.h>
#include<hgl/log/Log.h>
#include<string.h>

namespace hgl::graph
{
    bool ReadbackBuffer(VulkanDevice *device, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, void *out_data)
    {
        if (!device || buffer == VK_NULL_HANDLE || size == 0 || !out_data)
            return false;

        // 读回前先排空图形队列，避免读到进行中的写入
        VkResult queue_wait_result=vkQueueWaitIdle(device->GetGraphicsQueue());
        if(queue_wait_result!=VK_SUCCESS)
        {
            GLogError("[BufferReadback] vkQueueWaitIdle failed result=%d",
                      static_cast<int>(queue_wait_result));
            return false;
        }

        DeviceBuffer *staging = device->CreateBuffer(
            ObjectNameBuilder(AnsiString("BufferReadback:Staging")),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, size, nullptr,
            BufferAllocPolicy::Readback, SharingMode::Exclusive);
        if (!staging)
            return false;

        TextureCmdBuffer *cmd = device->CreateTextureCommandBuffer(
            ObjectNameBuilder(AnsiString("BufferReadback:Cmd")));
        if (!cmd)
        {
            delete staging;
            return false;
        }

        bool ok = cmd->Begin();

        if (ok)
        {
            VkBufferCopy region{};
            region.srcOffset = offset;
            region.dstOffset = 0;
            region.size = size;

            vkCmdCopyBuffer(*cmd, buffer, staging->GetBuffer(), 1, &region);

            ok = cmd->End();
        }

        if (ok)
        {
            const VkCommandBuffer raw_cmd = *cmd;

            VkFenceCreateInfo fi{};
            fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

            VkFence fence = VK_NULL_HANDLE;

            if (vkCreateFence(device->GetDevice(), &fi, nullptr, &fence) != VK_SUCCESS)
            {
                GLogError("[BufferReadback] vkCreateFence failed cmd=%p", (void*)raw_cmd);
                ok = false;
            }
            else
            {
                VkSubmitInfo si{};
                si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                si.commandBufferCount = 1;
                si.pCommandBuffers = &raw_cmd;

                VkResult submit_result=vkQueueSubmit(device->GetGraphicsQueue(), 1, &si, fence);
                if(submit_result!=VK_SUCCESS)
                {
                    GLogError("[BufferReadback] vkQueueSubmit failed result=%d cmd=%p fence=%p",
                              static_cast<int>(submit_result),(void*)raw_cmd,(void*)fence);
                    ok=false;
                }

                if (ok)
                {
                    VkResult wait_result=vkWaitForFences(device->GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX);
                    if(wait_result!=VK_SUCCESS)
                    {
                        GLogError("[BufferReadback] vkWaitForFences failed result=%d cmd=%p fence=%p",
                                  static_cast<int>(wait_result),(void*)raw_cmd,(void*)fence);
                        ok=false;
                    }
                }

                vkDestroyFence(device->GetDevice(), fence, nullptr);
            }
        }

        if (ok)
        {
            const void *mapped = staging->GetGPUBuffer()->Map(0, size);
            if (mapped)
            {
                memcpy(out_data, mapped, static_cast<size_t>(size));
                staging->GetGPUBuffer()->Unmap();
            }
            else
            {
                ok = false;
            }
        }

        delete cmd;
        delete staging;
        return ok;
    }

    bool ReadbackBuffer(DeviceBuffer *buffer, VkDeviceSize offset, VkDeviceSize size, void *out_data)
    {
        if (!buffer || size == 0 || !out_data)
            return false;

        // 如果可直接 Map，先尝试直接映射读取
        void *mapped = buffer->GetGPUBuffer() ? buffer->GetGPUBuffer()->Map(offset, size) : buffer->Map(offset, size);
        if (mapped)
        {
            memcpy(out_data, mapped, static_cast<size_t>(size));
            if (buffer->GetGPUBuffer())
                buffer->GetGPUBuffer()->Unmap();
            else
                buffer->Unmap();
            return true;
        }

        VulkanDevice *device = buffer->GetVkDevice() ? VulkanDevice::FromDevice(buffer->GetVkDevice()) : nullptr;
        if (!device)
            return false;

        return ReadbackBuffer(device, buffer->GetBuffer(), offset, size, out_data);
    }

    bool ReadbackBuffer(DeviceBuffer *buffer, void *out_data)
    {
        if (!buffer)
            return false;
        return ReadbackBuffer(buffer, 0, buffer->GetSize(), out_data);
    }

}
