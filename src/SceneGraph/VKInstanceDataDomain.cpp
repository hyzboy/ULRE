#include<hgl/vk/VKInstanceDataDomain.h>
#include<hgl/type/ActiveMemoryBlockManager.h>
#include<hgl/graph/module/BufferManager.h>

#include<algorithm>

namespace
{
    static uint32_t NextPow2(uint32_t v)
    {
        if (v <= 1u)
            return 1u;

        --v;
        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        return v + 1u;
    }

    static void *MapWritable(hgl::graph::DeviceBuffer *buf, VkDeviceSize offset, VkDeviceSize size)
    {
        if (!buf)
            return nullptr;

        if (auto *gpu = buf->GetGPUBuffer())
            return gpu->Map(offset, size);

        return buf->Map(offset, size);
    }

    static void UnmapWritable(hgl::graph::DeviceBuffer *buf)
    {
        if (!buf)
            return;

        if (auto *gpu = buf->GetGPUBuffer())
        {
            gpu->Unmap();
            return;
        }

        buf->Unmap();
    }
}

hgl::graph::InstanceDataDomain::InstanceDataDomain(
    hgl::graph::mtl::InstanceDataLayout layout,
    uint32_t max_count,
    uint8_t tex_array_slots)
{
    instance_layout         = layout;
    max_count            = max_count;
    texture_array_slot_flags = tex_array_slots;

    const uint32_t stride = hgl::graph::mtl::GetInstanceDataStride(layout);
    if(stride > 0)
        data_manager = new hgl::ActiveMemoryBlockManager(stride);
}

hgl::graph::InstanceDataDomain::~InstanceDataDomain()
{
    if (buffer_manager)
    {
        if (gpu_buffer)
        {
            buffer_manager->Release(gpu_buffer);
            gpu_buffer = nullptr;
        }

        if (mit_gpu_buffer)
        {
            buffer_manager->Release(mit_gpu_buffer);
            mit_gpu_buffer = nullptr;
        }
    }

    delete data_manager;
    data_manager = nullptr;
}

int hgl::graph::InstanceDataDomain::AllocSlot()
{
    if(!data_manager)
        return -1;

    int mi_id = -1;
    data_manager->GetOrCreate(&mi_id, 1);
    return mi_id;
}

void hgl::graph::InstanceDataDomain::FreeSlot(int mi_id)
{
    if(mi_id < 0 || !data_manager)
        return;

    data_manager->Release(&mi_id, 1);
}

void *hgl::graph::InstanceDataDomain::GetSlotData(int mi_id)
{
    if(!data_manager)
        return nullptr;

    return data_manager->GetData(mi_id);
}

bool hgl::graph::InstanceDataDomain::EnsureGPUBuffer(BufferManager *bm, uint32_t min_mi_count, bool allow_recreate)
{
    if (!bm)
        return false;

    const uint32_t stride = GetDataStride();
    if (stride == 0)
        return false;

    const uint32_t required_count = std::max(min_mi_count, max_count);
    if (required_count == 0)
        return false;

    if (gpu_buffer && gpu_capacity >= required_count)
    {
        buffer_manager = bm;
        return true;
    }

    if (gpu_buffer && !allow_recreate)
        return false;

    const uint32_t new_capacity = NextPow2(required_count);
    DeviceBuffer *new_buffer = bm->CreateSSBO("Domain:MIData",
                                              static_cast<VkDeviceSize>(stride) * new_capacity,
                                              nullptr,
                                              SharingMode::Exclusive);
    if (!new_buffer)
        return false;

    if (gpu_buffer)
        bm->Release(gpu_buffer);

    buffer_manager = bm;
    gpu_buffer = new_buffer;
    gpu_capacity = new_capacity;

    // New buffer means GPU content is undefined; request full refresh for current capacity.
    MarkDirtyRange(0, new_capacity);
    ++full_upload_fallback_count;
    return true;
}

bool hgl::graph::InstanceDataDomain::EnsureMITBuffer(BufferManager *bm, uint32_t min_uint_count, bool allow_recreate)
{
    if (!bm)
        return false;

    if (min_uint_count == 0)
        return false;

    if (mit_gpu_buffer && mit_gpu_capacity >= min_uint_count)
    {
        buffer_manager = bm;
        return true;
    }

    if (mit_gpu_buffer && !allow_recreate)
        return false;

    const uint32_t new_capacity = NextPow2(min_uint_count);
    DeviceBuffer *new_buffer = bm->CreateSSBO("Domain:MITData",
                                              static_cast<VkDeviceSize>(sizeof(uint32_t)) * new_capacity,
                                              nullptr,
                                              SharingMode::Exclusive);
    if (!new_buffer)
        return false;

    if (mit_gpu_buffer)
        bm->Release(mit_gpu_buffer);

    buffer_manager = bm;
    mit_gpu_buffer = new_buffer;
    mit_gpu_capacity = new_capacity;

    MarkMITDirtyRange(0, new_capacity);
    ++mit_full_upload_fallback_count;
    return true;
}

void hgl::graph::InstanceDataDomain::MarkDirtyRange(uint32_t begin, uint32_t count)
{
    if (count == 0)
        return;

    const uint32_t end = begin + count;
    if (!dirty)
    {
        dirty = true;
        dirty_begin = begin;
        dirty_end = end;
        return;
    }

    dirty_begin = std::min(dirty_begin, begin);
    dirty_end = std::max(dirty_end, end);
}

void hgl::graph::InstanceDataDomain::MarkMITDirtyRange(uint32_t begin, uint32_t count)
{
    if (count == 0)
        return;

    const uint32_t end = begin + count;
    if (!mit_dirty)
    {
        mit_dirty = true;
        mit_dirty_begin = begin;
        mit_dirty_end = end;
        return;
    }

    mit_dirty_begin = std::min(mit_dirty_begin, begin);
    mit_dirty_end = std::max(mit_dirty_end, end);
}

void hgl::graph::InstanceDataDomain::ClearDirtyRange()
{
    dirty = false;
    dirty_begin = 0;
    dirty_end = 0;
}

void hgl::graph::InstanceDataDomain::ClearMITDirtyRange()
{
    mit_dirty = false;
    mit_dirty_begin = 0;
    mit_dirty_end = 0;
}

bool hgl::graph::InstanceDataDomain::UploadDirtyRange()
{
    if (!dirty)
        return true;

    if (!gpu_buffer || !data_manager)
        return false;

    const uint32_t stride = GetDataStride();
    if (stride == 0)
        return false;

    if (dirty_begin >= dirty_end)
    {
        ClearDirtyRange();
        return true;
    }

    if (dirty_begin >= gpu_capacity)
    {
        ClearDirtyRange();
        return false;
    }

    const uint32_t clamped_end = std::min(dirty_end, gpu_capacity);
    const uint32_t copy_count = clamped_end - dirty_begin;
    if (copy_count == 0)
    {
        ClearDirtyRange();
        return true;
    }

    const VkDeviceSize byte_offset = static_cast<VkDeviceSize>(dirty_begin) * stride;
    const VkDeviceSize byte_size = static_cast<VkDeviceSize>(copy_count) * stride;

    uint8_t *dst = static_cast<uint8_t *>(MapWritable(gpu_buffer, byte_offset, byte_size));
    if (!dst)
        return false;

    for (uint32_t i = 0; i < copy_count; ++i)
    {
        const uint32_t mi_id = dirty_begin + i;
        const void *src = data_manager->GetData(mi_id);
        if (src)
            memcpy(dst + static_cast<size_t>(i) * stride, src, stride);
        else
            memset(dst + static_cast<size_t>(i) * stride, 0, stride);
    }

    UnmapWritable(gpu_buffer);
    uploaded_bytes_total += static_cast<uint64_t>(byte_size);
    ClearDirtyRange();
    return true;
}

bool hgl::graph::InstanceDataDomain::UploadMITDirtyRange(const uint32_t *mit_source_data, uint32_t mit_source_count)
{
    if (!mit_dirty)
        return true;

    if (!mit_gpu_buffer || !mit_source_data || mit_source_count == 0)
        return false;

    if (mit_dirty_begin >= mit_dirty_end)
    {
        ClearMITDirtyRange();
        return true;
    }

    if (mit_dirty_begin >= mit_gpu_capacity || mit_dirty_begin >= mit_source_count)
    {
        ClearMITDirtyRange();
        return false;
    }

    const uint32_t clamped_end = std::min({mit_dirty_end, mit_gpu_capacity, mit_source_count});
    const uint32_t copy_count = clamped_end - mit_dirty_begin;
    if (copy_count == 0)
    {
        ClearMITDirtyRange();
        return true;
    }

    const VkDeviceSize byte_offset = static_cast<VkDeviceSize>(mit_dirty_begin) * sizeof(uint32_t);
    const VkDeviceSize byte_size = static_cast<VkDeviceSize>(copy_count) * sizeof(uint32_t);

    void *dst = MapWritable(mit_gpu_buffer, byte_offset, byte_size);
    if (!dst)
        return false;

    memcpy(dst, mit_source_data + mit_dirty_begin, static_cast<size_t>(byte_size));
    UnmapWritable(mit_gpu_buffer);

    mit_uploaded_bytes_total += static_cast<uint64_t>(byte_size);
    ClearMITDirtyRange();
    return true;
}
