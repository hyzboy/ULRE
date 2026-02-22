#pragma once

#include<hgl/vk/DeviceBufferRingWriter.h>

namespace hgl::graph
{
class RingBufferWrapper
{
private:
    DeviceBufferRingWriter writer;
    bool dirty = false;

public:
    RingBufferWrapper(DeviceBuffer *buf = nullptr,
                      VkDeviceSize element_size = 0,
                      uint32_t ring_frames = HGL_L2W_RING_FRAMES)
        : writer(buf, element_size, ring_frames)
    {
    }

    void SetBuffer(DeviceBuffer *buf) { writer.SetBuffer(buf); }
    void SetElementSize(VkDeviceSize size) { writer.SetElementSize(size); }
    void SetFrameIndex(uint32_t frame_index) { writer.SetFrameIndex(frame_index); }
    void AdvanceFrame() { writer.AdvanceFrame(); }

    uint32_t GetFrameIndex() const { return writer.GetFrameIndex(); }
    uint32_t GetRingFrames() const { return writer.GetRingFrames(); }
    uint32_t GetBaseIndex(uint32_t static_count, uint32_t dynamic_count) const
    {
        return writer.GetBaseIndex(static_count, dynamic_count);
    }
    uint32_t GetTotalCount(uint32_t static_count, uint32_t dynamic_count) const
    {
        return writer.GetTotalCount(static_count, dynamic_count);
    }
    VkDeviceSize GetBaseOffsetBytes(uint32_t static_count, uint32_t dynamic_count) const
    {
        return writer.GetBaseOffsetBytes(static_count, dynamic_count);
    }
    VkDeviceSize GetDynamicSizeBytes(uint32_t dynamic_count) const
    {
        return writer.GetDynamicSizeBytes(dynamic_count);
    }
    VkDeviceSize GetTotalSizeBytes(uint32_t static_count, uint32_t dynamic_count) const
    {
        return writer.GetTotalSizeBytes(static_count, dynamic_count);
    }

    void *MapDynamicRange(uint32_t static_count, uint32_t dynamic_count)
    {
        void *ptr = writer.MapDynamicRange(static_count, dynamic_count);
        if(ptr)
            dirty = true;
        return ptr;
    }

    void Unmap()
    {
        writer.Unmap();
    }

    bool WriteDynamicRange(const void *data, uint32_t static_count, uint32_t dynamic_count)
    {
        const bool ok = writer.WriteDynamicRange(data, static_count, dynamic_count);
        if(ok)
            dirty = true;
        return ok;
    }

    bool IsDirty() const { return dirty; }
    void ClearDirty() { dirty = false; }
};
}
