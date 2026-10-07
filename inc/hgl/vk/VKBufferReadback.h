#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/type/String.h>
#include<hgl/type/ValueArray.h>

namespace hgl::graph
{
    /**
     * GPU 缓冲区读回（GPU → CPU）与 SSBO DUMP 诊断工具。
     *
     * 1. 若缓冲区已分配为 HostVisible 内存，可直接 Map 读回。
     * 2. 若缓冲区为 DeviceLocal，创建 Readback Staging 缓冲并提交拷贝命令同步读回。
     * 3. 支持将 SSBO 格式化 Dump 输出到日志。
     */
    bool ReadbackBuffer(VulkanDevice *device, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, void *out_data);

    bool ReadbackBuffer(DeviceBuffer *buffer, VkDeviceSize offset, VkDeviceSize size, void *out_data);

    bool ReadbackBuffer(DeviceBuffer *buffer, void *out_data);

    /**
     * 将缓冲内容读回为 uint32 数组
     */
    bool ReadbackBufferU32(DeviceBuffer *buffer, VkDeviceSize offset, uint32_t count, ValueArray<uint32_t> &out_values);

    /**
     * 将 SSBO 的部分数据格式化打印到日志
     * @param tag 诊断日志标签
     * @param buffer 要 dump 的缓冲
     * @param offset 字节起始偏移
     * @param count uint32 元素数量
     * @param elements_per_line 每行显示的元素数
     */
    bool DumpSSBOU32ToLog(const char *tag, DeviceBuffer *buffer, VkDeviceSize offset, uint32_t count, uint32_t elements_per_line = 8);

    /**
     * 将 SSBO 以 Hex 形式格式化打印到日志
     */
    bool DumpSSBOHexToLog(const char *tag, DeviceBuffer *buffer, VkDeviceSize offset, VkDeviceSize size, uint32_t bytes_per_line = 16);
}
