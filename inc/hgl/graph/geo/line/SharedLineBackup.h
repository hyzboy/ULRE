#pragma once

#include <hgl/math/Vector.h>
#include <hgl/type/DataArray.h>

namespace hgl::graph
{
    // Shared temporary backup buffers for LineWidthBatch
    struct SharedLineBackup
    {
        DataArray<Vector3f> positions;
        DataArray<uint8>  colors;

        size_t max_reserved = 0;

        void EnsureCapacity(size_t vertex_count)
        {
            if(vertex_count > max_reserved)
            {
                positions.Reserve(vertex_count);
                colors.Reserve(vertex_count);
                max_reserved = vertex_count;
            }
        }

        void Clear()
        {
            positions.Clear();
            colors.Clear();
        }

        bool IsEmpty() const
        {
            return positions.IsEmpty();
        }
    };
}
