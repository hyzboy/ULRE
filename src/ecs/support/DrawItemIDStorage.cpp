#include <hgl/ecs/support/DrawItemIDStorage.h>
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

    void DrawItemIDStorage::SetExternalGPUBuffer(graph::DeviceBuffer *buf, graph::VulkanDevice *dev)
    {
        if (buf && buf->GetBuffer() && dev)
        {
            external_gpu_address = dev->GetBufferDeviceAddressAligned16(buf->GetBuffer());
        }
        else
        {
            external_gpu_address = 0;
        }
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
        if (!dev)
            return false;

        if (device_buffer && gpu_capacity >= min_capacity && min_capacity > 0)
            return true;

        uint32_t target_capacity = min_capacity > 0 ? min_capacity : 1024;
        if (gpu_capacity > 0)
        {
            target_capacity = std::max(target_capacity, gpu_capacity * 2);
        }
        if (target_capacity < 64)
            target_capacity = 64;

        const VkDeviceSize byte_size = sizeof(uint32_t) * target_capacity;

        graph::DeviceBuffer *new_buffer = dev->CreateSSBO(byte_size,
                                                          nullptr,
                                                          graph::BufferAllocPolicy::Auto,
                                                          graph::SharingMode::Exclusive);
        if (!new_buffer)
        {
            LogError(u8"[DrawItemIDStorage] 创建 SSBO 失败, 目标容量: %u, 字节数: %llu",
                     target_capacity,
                     byte_size);
            return false;
        }

        if (device_buffer)
        {
            delete device_buffer;
            device_buffer = nullptr;
        }

        device_buffer = new_buffer;
        gpu_capacity = target_capacity;
        gpu_address = dev->GetBufferDeviceAddressAligned16(device_buffer->GetBuffer());

        LogInfo(u8"[DrawItemIDStorage] 扩容 GPU SSBO 成功: 容量 %u, 地址: 0x%llx",
                gpu_capacity,
                gpu_address);
        return true;
    }

    bool DrawItemIDStorage::SyncToGPU(graph::VulkanDevice *dev)
    {
        if (external_gpu_address != 0)
        {
            last_frame_uploaded_bytes = 0;
            return true;
        }

        if (!dev)
            return false;

        const uint32_t count = static_cast<uint32_t>(ids.GetCount());
        last_frame_item_count = count;

        if (count == 0)
        {
            last_frame_uploaded_bytes = 0;
            return true;
        }

        if (!EnsureGPUBuffer(dev, count))
            return false;

        const VkDeviceSize byte_size = static_cast<VkDeviceSize>(count) * sizeof(uint32_t);
        device_buffer->Write(ids.GetData(), 0, static_cast<uint32_t>(byte_size));
        last_frame_uploaded_bytes = static_cast<uint32_t>(byte_size);
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
        last_frame_uploaded_bytes = 0;
        last_frame_item_count = 0;
    }
}
