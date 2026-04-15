#include <hgl/vk/MITBuffer.h>
#include <hgl/graph/module/BufferManager.h>

#include <algorithm>
#include <cstring>

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

namespace hgl::graph {

MITBuffer::~MITBuffer()
{
    if (buffer_manager && gpu_buffer)
    {
        buffer_manager->Release(gpu_buffer);
        gpu_buffer = nullptr;
    }
}

bool MITBuffer::EnsureBuffer(BufferManager *bm, uint32_t min_uint_count, bool allow_recreate)
{
    if (!bm)
        return false;

    if (min_uint_count == 0)
        return false;

    if (gpu_buffer && gpu_capacity >= min_uint_count)
    {
        buffer_manager = bm;
        return true;
    }

    if (gpu_buffer && !allow_recreate)
        return false;

    const uint32_t new_capacity = NextPow2(min_uint_count);
    DeviceBuffer *new_buffer = bm->CreateSSBO("Domain:MITData",
                                              static_cast<VkDeviceSize>(sizeof(uint32_t)) * new_capacity,
                                              nullptr,
                                              SharingMode::Exclusive);
    if (!new_buffer)
        return false;

    if (gpu_buffer)
        bm->Release(gpu_buffer);

    buffer_manager = bm;
    gpu_buffer     = new_buffer;
    gpu_capacity   = new_capacity;

    MarkDirtyRange(0, new_capacity);
    ++full_upload_fallback_count;
    return true;
}

void MITBuffer::MarkDirtyRange(uint32_t begin, uint32_t count)
{
    if (count == 0)
        return;

    const uint32_t end = begin + count;
    if (!dirty)
    {
        dirty       = true;
        dirty_begin = begin;
        dirty_end   = end;
        return;
    }

    dirty_begin = std::min(dirty_begin, begin);
    dirty_end   = std::max(dirty_end, end);
}

void MITBuffer::ClearDirtyRange()
{
    dirty       = false;
    dirty_begin = 0;
    dirty_end   = 0;
}

bool MITBuffer::UploadDirtyRange(const uint32_t *source_data, uint32_t source_count)
{
    if (!dirty)
        return true;

    if (!gpu_buffer || !source_data || source_count == 0)
        return false;

    if (dirty_begin >= dirty_end)
    {
        ClearDirtyRange();
        return true;
    }

    if (dirty_begin >= gpu_capacity || dirty_begin >= source_count)
    {
        ClearDirtyRange();
        return false;
    }

    const uint32_t clamped_end = std::min({dirty_end, gpu_capacity, source_count});
    const uint32_t copy_count  = clamped_end - dirty_begin;
    if (copy_count == 0)
    {
        ClearDirtyRange();
        return true;
    }

    const VkDeviceSize byte_offset = static_cast<VkDeviceSize>(dirty_begin) * sizeof(uint32_t);
    const VkDeviceSize byte_size   = static_cast<VkDeviceSize>(copy_count)  * sizeof(uint32_t);

    void *dst = MapWritable(gpu_buffer, byte_offset, byte_size);
    if (!dst)
        return false;

    memcpy(dst, source_data + dirty_begin, static_cast<size_t>(byte_size));
    UnmapWritable(gpu_buffer);

    uploaded_bytes_total += static_cast<uint64_t>(byte_size);
    ClearDirtyRange();
    return true;
}

} // namespace hgl::graph
