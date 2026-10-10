#include <hgl/ecs/support/DrawItemIDStorage.h>
#include <hgl/ecs/support/GpuMirrorBuffer.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <algorithm>

namespace hgl::ecs
{
    DrawItemIDStorage::DrawItemIDStorage()
    {
        ids.Reserve(256);
    }

    DrawItemIDStorage::~DrawItemIDStorage()
    {
        ReleaseGPUBuffer();
    }

    void DrawItemIDStorage::Reset()
    {
        ids.Clear();
        is_dirty = false;
        external_gpu_address = 0;
    }

    uint32_t DrawItemIDStorage::Append(const uint32_t *handles, const uint32_t count)
    {
        if (!handles || count == 0)
            return static_cast<uint32_t>(ids.GetCount());

        const uint32_t offset = static_cast<uint32_t>(ids.GetCount());
        for (uint32_t i = 0; i < count; ++i)
        {
            ids.Add(handles[i]);
        }

        is_dirty = true;
        return offset;
    }

    uint32_t DrawItemIDStorage::Append(const uint32_t handle)
    {
        const uint32_t offset = static_cast<uint32_t>(ids.GetCount());
        ids.Add(handle);
        is_dirty = true;
        return offset;
    }

    bool DrawItemIDStorage::EnsureGPUBuffer(graph::VulkanDevice *dev, const uint32_t min_capacity)
    {
        // 策略 / 建 SSBO / 换缓冲 / 记容量与 BDA 走公共件（审计 §2.2：与 `RenderItemDataStorage`
        // 逐字同源的那一份已删）；两条日志留在本表（对象日志宏带 `typeid(*this)`，文本逐字不变）。
        // 本表无"迁移已有行"需求：`SyncToGPU` 每次都整段重写 CPU 数组。
        uint32_t target_capacity = 0;

        const GpuMirrorResult result = EnsureGpuMirrorBuffer(dev,
                                                             device_buffer,
                                                             gpu_capacity,
                                                             gpu_address,
                                                             min_capacity,
                                                             sizeof(uint32_t),
                                                             target_capacity,
                                                             [](graph::DeviceBuffer *, uint32_t) {});

        if (result == GpuMirrorResult::NoDevice)
            return false;

        if (result == GpuMirrorResult::Unchanged)
            return true;

        if (result == GpuMirrorResult::Failed)
        {
            LogError(u8"[DrawItemIDStorage] 创建 SSBO 失败, 目标容量: %u, 字节数: %llu",
                     target_capacity,
                     static_cast<VkDeviceSize>(sizeof(uint32_t)) * target_capacity);
            return false;
        }

        LogInfo(u8"[DrawItemIDStorage] 扩容 GPU SSBO 成功: 容量 %u, 地址: 0x%llx",
                gpu_capacity,
                gpu_address);
        return true;
    }

    bool DrawItemIDStorage::SyncToGPU(graph::VulkanDevice *dev)
    {
        if (external_gpu_address != 0)
            return true;

        if (!dev)
            return false;

        const uint32_t count = static_cast<uint32_t>(ids.GetCount());

        if (count == 0)
            return true;

        if (!EnsureGPUBuffer(dev, count))
            return false;

        const VkDeviceSize byte_size = static_cast<VkDeviceSize>(count) * sizeof(uint32_t);
        device_buffer->Write(ids.GetData(), 0, static_cast<uint32_t>(byte_size));
        is_dirty = false;
        return true;
    }

    void DrawItemIDStorage::ReleaseGPUBuffer()
    {
        if (device_buffer)
        {
            delete device_buffer;
            device_buffer = nullptr;
        }
        gpu_capacity = 0;
        gpu_address = 0;
    }
}
