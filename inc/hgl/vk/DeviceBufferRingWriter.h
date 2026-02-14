#pragma once

#include<hgl/graph/render/RenderOptions.h>
#include<hgl/vk/VKBuffer.h>

namespace hgl::graph
{
    class DeviceBufferRingWriter
    {
        DeviceBuffer *buffer;
        uint32_t ring_frames;
        uint32_t frame_index;
        VkDeviceSize element_size;

    public:
        DeviceBufferRingWriter(DeviceBuffer *buf = nullptr,
                               const VkDeviceSize elem_size = 0,
                               const uint32_t frames = HGL_L2W_RING_FRAMES)
            : buffer(buf)
            , ring_frames(frames ? frames : 1)
            , frame_index(0)
            , element_size(elem_size)
        {
        }

        void SetBuffer(DeviceBuffer *buf)
        {
            buffer = buf;
        }

        void SetElementSize(const VkDeviceSize elem_size)
        {
            element_size = elem_size;
        }

        void SetFrameIndex(const uint32_t index)
        {
            frame_index = ring_frames ? (index % ring_frames) : 0;
        }

        void AdvanceFrame()
        {
            frame_index = ring_frames ? ((frame_index + 1) % ring_frames) : 0;
        }

        uint32_t GetFrameIndex() const
        {
            return frame_index;
        }

        uint32_t GetRingFrames() const
        {
            return ring_frames;
        }

        uint32_t GetBaseIndex(const uint32_t static_count, const uint32_t dynamic_count) const
        {
            return static_count + frame_index * dynamic_count;
        }

        uint32_t GetTotalCount(const uint32_t static_count, const uint32_t dynamic_count) const
        {
            return static_count + dynamic_count * ring_frames;
        }

        VkDeviceSize GetBaseOffsetBytes(const uint32_t static_count, const uint32_t dynamic_count) const
        {
            return static_cast<VkDeviceSize>(GetBaseIndex(static_count, dynamic_count)) * element_size;
        }

        VkDeviceSize GetDynamicSizeBytes(const uint32_t dynamic_count) const
        {
            return static_cast<VkDeviceSize>(dynamic_count) * element_size;
        }

        VkDeviceSize GetTotalSizeBytes(const uint32_t static_count, const uint32_t dynamic_count) const
        {
            return static_cast<VkDeviceSize>(GetTotalCount(static_count, dynamic_count)) * element_size;
        }

        void *MapDynamicRange(const uint32_t static_count, const uint32_t dynamic_count)
        {
            if (!buffer || element_size == 0 || dynamic_count == 0)
                return nullptr;

            return buffer->Map(GetBaseOffsetBytes(static_count, dynamic_count),
                               GetDynamicSizeBytes(dynamic_count));
        }

        void Unmap()
        {
            if (buffer)
                buffer->Unmap();
        }

        bool WriteDynamicRange(const void *data, const uint32_t static_count, const uint32_t dynamic_count)
        {
            if (!buffer || element_size == 0 || dynamic_count == 0)
                return false;

            const VkDeviceSize offset = GetBaseOffsetBytes(static_count, dynamic_count);
            const VkDeviceSize size = GetDynamicSizeBytes(dynamic_count);
            return buffer->Write(data, static_cast<uint32_t>(offset), static_cast<uint32_t>(size));
        }
    };
}
