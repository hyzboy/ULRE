#include<hgl/ecs/core/RenderItem.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>

namespace hgl::ecs
{
    // RenderItem base class implementation
    int RenderItem::Compare(const RenderItem& other) const
    {
        // Compare by geometry binding first (for batching).
        // Must use content comparison (operator<=>) NOT pointer comparison:
        // EnsureRuntimeGeometryBinding allocates a new GeometryDataBuffer per component,
        // so same-geometry items have different pointers but identical content.
        // Pointer-based sort would scatter same-geometry items → no instancing merge.
        auto *data_1 = GetGeometryDataBuffer();
        auto *data_2 = other.GetGeometryDataBuffer();
        if (data_1 && data_2)
        {
            if (auto cmp = *data_1 <=> *data_2; cmp != 0)
                return cmp < 0 ? -1 : 1;
        }
        else if (data_1 != data_2)
        {
            return data_1 ? 1 : -1;
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
