#pragma once

#include <cstdint>

namespace hgl::graph
{

class BufferManager;
class DeviceBuffer;

/**
 * MITBuffer — Material Instance Texture ID GPU 缓冲区
 *
 * 轻量 buffer host：不管理 CPU 端数据，仅负责 GPU SSBO 的
 * 创建、dirty range 追踪和上传。CPU 端数据由调用方（Primitive）持有。
 */
class MITBuffer
{
    BufferManager *buffer_manager = nullptr;

    DeviceBuffer  *gpu_buffer  = nullptr;
    uint32_t       gpu_capacity = 0;      ///< MIT uint32 capacity

    bool     dirty       = false;
    uint32_t dirty_begin = 0;
    uint32_t dirty_end   = 0;             ///< one past end

    uint64_t uploaded_bytes_total        = 0;
    uint32_t full_upload_fallback_count  = 0;

public:

    MITBuffer() = default;
    ~MITBuffer();

    bool EnsureBuffer(BufferManager *bm, uint32_t min_uint_count, bool allow_recreate = true);

    DeviceBuffer *GetGPUBuffer()  const { return gpu_buffer; }
    uint32_t GetGPUCapacity()     const { return gpu_capacity; }

    bool     HasDirtyRange()  const { return dirty; }
    uint32_t GetDirtyBegin()  const { return dirty_begin; }
    uint32_t GetDirtyEnd()    const { return dirty_end; }

    void MarkDirtyRange(uint32_t begin, uint32_t count);
    void ClearDirtyRange();

    bool UploadDirtyRange(const uint32_t *source_data, uint32_t source_count);

    uint64_t GetUploadedBytesTotal()       const { return uploaded_bytes_total; }
    uint32_t GetFullUploadFallbackCount()  const { return full_upload_fallback_count; }
}; // class MITBuffer

} // namespace hgl::graph
