#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/type/String.h>
#include<hgl/type/ValueArray.h>

namespace hgl::graph
{
    /**
     * GPU 缓冲区读回（GPU → CPU）。
     *
     * 1. 若缓冲区已分配为 HostVisible 内存，可直接 Map 读回。
     * 2. 若缓冲区为 DeviceLocal，创建 Readback Staging 缓冲并提交拷贝命令同步读回。
     */
    bool ReadbackBuffer(VulkanDevice *device, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, void *out_data);

    bool ReadbackBuffer(DeviceBuffer *buffer, VkDeviceSize offset, VkDeviceSize size, void *out_data);

    bool ReadbackBuffer(DeviceBuffer *buffer, void *out_data);
}
