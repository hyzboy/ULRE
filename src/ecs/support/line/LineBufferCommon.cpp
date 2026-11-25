#include <hgl/ecs/support/line/LineBufferCommon.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/graph/mesh/GeometryDataBuffer.h>
#include <hgl/graph/mesh/GeometryDrawRange.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/log/Log.h>
#include <hgl/common/DebugChannel.h>

namespace hgl::ecs::detail
{
    bool ComputeLineBufferTargetCapacity(uint64_t needed,
                                         uint32_t granule,
                                         uint32_t current_capacity,
                                         uint32_t &out_capacity)
    {
        const uint64_t g = granule ? granule : 1u;
        const uint64_t rounded_required = ((needed + g - 1u) / g) * g;
        uint64_t target = rounded_required;

        if (current_capacity)
        {
            const uint64_t doubled = static_cast<uint64_t>(current_capacity) * 2u;
            if (target < doubled)
                target = doubled;
        }

        target = ((target + g - 1u) / g) * g;
        if (target > kLineBufferMaxCapacity)
            target = rounded_required;

        if (target > kLineBufferMaxCapacity)
            return false;

        out_capacity = static_cast<uint32_t>(target);
        return true;
    }

    void LineBufferBackoff::Reset()
    {
        failed_capacity    = 0;
        retry_delay_frames = 0;
        last_failure_frame = 0;
    }

    bool LineBufferBackoff::ShouldSkip(uint32_t target_capacity, uint32_t frame_serial) const
    {
        return failed_capacity == target_capacity
            && retry_delay_frames > 0
            && static_cast<uint32_t>(frame_serial - last_failure_frame) < retry_delay_frames;
    }

    void LineBufferBackoff::RecordFailure(uint32_t target_capacity, uint32_t frame_serial)
    {
        if (failed_capacity != target_capacity)
        {
            failed_capacity    = target_capacity;
            retry_delay_frames = 1;
        }
        else if (retry_delay_frames < 60)
        {
            retry_delay_frames = retry_delay_frames > 30 ? 60 : retry_delay_frames * 2;
        }

        last_failure_frame = frame_serial;
    }

    bool LineBufferCandidate::Create(graph::VulkanDevice *dev,
                                     const graph::GeometryVertexFormat &format,
                                     const AnsiString &name,
                                     uint32_t capacity_lines,
                                     const char *log_tag)
    {
        geometry = graph::CreateGeometry(dev,
                                         format,
                                         name,
                                         capacity_lines * 2u,
                                         0,
                                         graph::IndexType::AUTO,
                                         nullptr,
                                         graph::BufferAllocPolicy::StagedUpload);
        if (!geometry)
        {
            GLogError("%s Failed to create replacement LineBuffer geometry (capacity=%u)", log_tag, capacity_lines);
            return false;
        }

        // 顶点输入统一为 SSBO：GeometryDataBuffer 槽位数 = Geometry 语义数
        const uint32_t semantic_count = geometry->GetGeometryVertexFormat().GetCount();

        data_buffer = new graph::GeometryDataBuffer(semantic_count,
                                                    geometry->GetIBO(),
                                                    geometry->GetVDM());
        if (!data_buffer || !data_buffer->Update(geometry))
        {
            GLogError("%s GeometryDataBuffer::Update failed (capacity=%u)", log_tag, capacity_lines);
            Discard();
            return false;
        }

        draw_range = new graph::GeometryDrawRange();
        if (!draw_range)
        {
            GLogError("%s Failed to create replacement LineBuffer draw range (capacity=%u)", log_tag, capacity_lines);
            Discard();
            return false;
        }

        draw_range->Set(geometry);
        draw_range->vertex_count = 0;
        return true;
    }

    void LineBufferCandidate::Discard()
    {
        SAFE_CLEAR(data_buffer);
        SAFE_CLEAR(draw_range);
        SAFE_CLEAR(geometry);
    }

    void LineBufferCandidate::Install(graph::Geometry *&out_geometry,
                                      graph::GeometryDataBuffer *&out_data_buffer,
                                      graph::GeometryDrawRange *&out_draw_range)
    {
        out_geometry    = geometry;
        out_data_buffer = data_buffer;
        out_draw_range  = draw_range;

        geometry    = nullptr;
        data_buffer = nullptr;
        draw_range  = nullptr;
    }
}//namespace hgl::ecs::detail
