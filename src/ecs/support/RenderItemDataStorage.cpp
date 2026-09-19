#include <hgl/ecs/support/RenderItemDataStorage.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <algorithm>

namespace hgl::ecs
{
    RenderItemDataStorage::RenderItemDataStorage()
    {
        // 初始预分配 64 项容量，避免反复扩容
        items.Reserve(64);
        free_list.Reserve(16);
    }

    RenderItemDataStorage::~RenderItemDataStorage()
    {
        ReleaseGPUBuffer();
    }

    RenderItemHandle RenderItemDataStorage::Allocate()
    {
        return Allocate(RenderItemDescriptor{});
    }

    RenderItemHandle RenderItemDataStorage::Allocate(const RenderItemDescriptor &desc)
    {
        if (free_list.GetCount() > 0)
        {
            const int last_idx = free_list.GetCount() - 1;
            const RenderItemHandle handle = free_list[last_idx];
            free_list.Delete(last_idx);

            items[handle] = desc;
            MarkDirty(handle);
            return handle;
        }

        const RenderItemHandle handle = static_cast<RenderItemHandle>(items.GetCount());
        items.Add(desc);
        MarkDirty(handle);
        return handle;
    }

    RenderItemHandle RenderItemDataStorage::AllocateContiguous(const uint32_t count)
    {
        if (count == 0)
            return INVALID_RENDER_ITEM_HANDLE;

        const RenderItemHandle base = static_cast<RenderItemHandle>(items.GetCount());
        for (uint32_t i = 0; i < count; ++i)
        {
            items.Add(RenderItemDescriptor{});
        }

        MarkRangeDirty(base, count);
        return base;
    }

    bool RenderItemDataStorage::Release(const RenderItemHandle handle)
    {
        if (handle >= static_cast<RenderItemHandle>(items.GetCount()))
            return false;

        items[handle] = RenderItemDescriptor{};
        free_list.Add(handle);
        MarkDirty(handle);
        return true;
    }

    bool RenderItemDataStorage::ReleaseContiguous(const RenderItemHandle base, const uint32_t count)
    {
        if (count == 0 || base + count > static_cast<RenderItemHandle>(items.GetCount()))
            return false;

        for (uint32_t i = 0; i < count; ++i)
        {
            const RenderItemHandle h = base + i;
            items[h] = RenderItemDescriptor{};
            free_list.Add(h);
        }

        MarkRangeDirty(base, count);
        return true;
    }

    void RenderItemDataStorage::Clear()
    {
        items.Clear();
        free_list.Clear();
        ClearDirty();
    }

    const RenderItemDescriptor *RenderItemDataStorage::Get(const RenderItemHandle handle) const
    {
        if (handle >= static_cast<RenderItemHandle>(items.GetCount()))
            return nullptr;

        return &items[handle];
    }

    RenderItemDescriptor *RenderItemDataStorage::Get(const RenderItemHandle handle)
    {
        if (handle >= static_cast<RenderItemHandle>(items.GetCount()))
            return nullptr;

        return &items[handle];
    }

    bool RenderItemDataStorage::Set(const RenderItemHandle handle, const RenderItemDescriptor &desc)
    {
        if (handle >= static_cast<RenderItemHandle>(items.GetCount()))
            return false;

        if (items[handle] != desc)
        {
            items[handle] = desc;
            MarkDirty(handle);
        }
        return true;
    }

    bool RenderItemDataStorage::SetTransformID(const RenderItemHandle handle, const uint32_t transform_id)
    {
        if (handle >= static_cast<RenderItemHandle>(items.GetCount()))
            return false;

        if (items[handle].transform_id != transform_id)
        {
            items[handle].transform_id = transform_id;
            MarkDirty(handle);
        }
        return true;
    }

    bool RenderItemDataStorage::SetGeometryID(const RenderItemHandle handle, const uint32_t geometry_id)
    {
        if (handle >= static_cast<RenderItemHandle>(items.GetCount()))
            return false;

        if (items[handle].geometry_id != geometry_id)
        {
            items[handle].geometry_id = geometry_id;
            MarkDirty(handle);
        }
        return true;
    }

    bool RenderItemDataStorage::SetMaterialID(const RenderItemHandle handle, const uint32_t material_id)
    {
        if (handle >= static_cast<RenderItemHandle>(items.GetCount()))
            return false;

        if (items[handle].material_id != material_id)
        {
            items[handle].material_id = material_id;
            MarkDirty(handle);
        }
        return true;
    }

    bool RenderItemDataStorage::SetTextureID(const RenderItemHandle handle, const uint32_t texture_id)
    {
        if (handle >= static_cast<RenderItemHandle>(items.GetCount()))
            return false;

        if (items[handle].texture_id != texture_id)
        {
            items[handle].texture_id = texture_id;
            MarkDirty(handle);
        }
        return true;
    }

    bool RenderItemDataStorage::Set4ID(const RenderItemHandle handle,
                                       const uint32_t transform_id,
                                       const uint32_t geometry_id,
                                       const uint32_t material_id,
                                       const uint32_t texture_id)
    {
        if (handle >= static_cast<RenderItemHandle>(items.GetCount()))
            return false;

        RenderItemDescriptor &item = items[handle];
        if (item.transform_id != transform_id ||
            item.geometry_id  != geometry_id  ||
            item.material_id  != material_id  ||
            item.texture_id   != texture_id)
        {
            item.transform_id = transform_id;
            item.geometry_id  = geometry_id;
            item.material_id  = material_id;
            item.texture_id   = texture_id;
            MarkDirty(handle);
        }
        return true;
    }

    bool RenderItemDataStorage::IsValidHandle(const RenderItemHandle handle) const
    {
        return handle < static_cast<RenderItemHandle>(items.GetCount());
    }

    void RenderItemDataStorage::MarkDirty(const uint32_t index)
    {
        if (index < dirty_min)
            dirty_min = index;
        if (index > dirty_max)
            dirty_max = index;
        is_dirty = true;
    }

    void RenderItemDataStorage::MarkRangeDirty(const uint32_t start, const uint32_t count)
    {
        if (count == 0)
            return;

        if (start < dirty_min)
            dirty_min = start;

        const uint32_t end = start + count - 1;
        if (end > dirty_max)
            dirty_max = end;

        is_dirty = true;
    }

    void RenderItemDataStorage::ClearDirty()
    {
        dirty_min = UINT32_MAX;
        dirty_max = 0;
        is_dirty = false;
    }

    bool RenderItemDataStorage::GetDirtyRange(uint32_t &out_min, uint32_t &out_max) const
    {
        if (!is_dirty)
            return false;

        out_min = dirty_min;
        out_max = dirty_max;
        return true;
    }

    bool RenderItemDataStorage::EnsureGPUBuffer(graph::VulkanDevice *dev, const uint32_t min_capacity)
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
        // 对齐至 2 的幂次或至少 64
        if (target_capacity < 64)
            target_capacity = 64;

        const VkDeviceSize byte_size = sizeof(RenderItemDescriptor) * target_capacity;

        graph::DeviceBuffer *new_buffer = dev->CreateSSBO(byte_size,
                                                         nullptr,
                                                         graph::BufferAllocPolicy::Auto,
                                                         graph::SharingMode::Exclusive);
        if (!new_buffer)
        {
            LogError(u8"[RenderItemDataStorage] 创建 SSBO 失败, 目标容量: %u, 字节数: %llu",
                     target_capacity,
                     byte_size);
            return false;
        }

        // 如果已有旧缓冲且存在数据，迁移至新缓冲
        if (items.GetCount() > 0)
        {
            const VkDeviceSize copy_size = sizeof(RenderItemDescriptor) * items.GetCount();
            new_buffer->Write(items.GetData(), 0, static_cast<uint32_t>(copy_size));
        }

        if (device_buffer)
        {
            delete device_buffer;
            device_buffer = nullptr;
        }

        device_buffer = new_buffer;
        gpu_capacity = target_capacity;
        gpu_address = dev->GetBufferDeviceAddressAligned16(device_buffer->GetBuffer());

        // 重新标记全量脏以确保 GPU 状态一致
        if (items.GetCount() > 0)
        {
            MarkRangeDirty(0, static_cast<uint32_t>(items.GetCount()));
        }

        LogInfo(u8"[RenderItemDataStorage] 扩容 GPU SSBO 成功: 容量 %u, 地址: 0x%llx",
                gpu_capacity,
                gpu_address);
        return true;
    }

    bool RenderItemDataStorage::SyncToGPU(graph::VulkanDevice *dev)
    {
        if (!dev)
            return false;

        const uint32_t item_count = static_cast<uint32_t>(items.GetCount());
        if (item_count == 0)
            return true;

        if (!device_buffer || gpu_capacity < item_count)
        {
            if (!EnsureGPUBuffer(dev, item_count))
                return false;
        }

        if (is_dirty && device_buffer)
        {
            const uint32_t start_idx = dirty_min;
            const uint32_t end_idx = std::min(dirty_max, item_count - 1);

            if (start_idx <= end_idx)
            {
                const uint32_t count = end_idx - start_idx + 1;
                const uint32_t offset = start_idx * sizeof(RenderItemDescriptor);
                const uint32_t size = count * sizeof(RenderItemDescriptor);

                device_buffer->Write(items.GetData() + start_idx, offset, size);
            }

            ClearDirty();
        }

        return true;
    }

    void RenderItemDataStorage::ReleaseGPUBuffer()
    {
        if (device_buffer)
        {
            delete device_buffer;
            device_buffer = nullptr;
        }
        gpu_capacity = 0;
        gpu_address = 0;
    }
}//namespace hgl::ecs
