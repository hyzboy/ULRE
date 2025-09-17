#pragma once

#include <vector>
#include <hgl/math/Vector.h>

namespace hgl::graph
{
    // Shared temporary backup buffers for LineWidthBatch
    struct SharedLineBackup
    {
        std::vector<Vector3f> positions;
        std::vector<uint8_t>  colors;

        size_t max_reserved = 0;

        void EnsureCapacity(size_t vertex_count)
        {
            if(vertex_count > max_reserved)
            {
                positions.reserve(vertex_count);
                colors.reserve(vertex_count);
                max_reserved = vertex_count;
            }
        }

        void Clear()
        {
            positions.clear();
            colors.clear();
        }

        bool Empty() const
        {
            return positions.empty();
        }
    };
}
