#include<hgl/ecs/core/RenderItem.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>

namespace hgl::ecs
{
    // RenderItem base class implementation
    int RenderItem::Compare(const RenderItem& other) const
    {
        // Compare by geometry binding first (for batching)
        auto *data_1 = GetGeometryDataBuffer();
        auto *data_2 = other.GetGeometryDataBuffer();
        if (data_1 != data_2)
        {
            if (data_1 < data_2) return -1;
            if (data_1 > data_2) return 1;
        }

        auto *range_1 = GetGeometryDrawRange();
        auto *range_2 = other.GetGeometryDrawRange();
        if (range_1 && range_2)
        {
            if (auto cmp = *range_1 <=> *range_2; cmp < 0) return -1;
            if (auto cmp = *range_1 <=> *range_2; cmp > 0) return 1;
        }

        // Then compare by distance to camera
        float diff = other.distanceToCamera - distanceToCamera;
        if (diff > 0.0f) return 1;
        if (diff < 0.0f) return -1;

        return 0;
    }
}//namespace hgl::ecs
