#include<hgl/graph/data/VertexLayout.h>

namespace hgl::graph::data
{
    size_t VertexLayout::GetVertexSize() const
    {
        size_t size = 0;
        
        if(has_position) size += 12;  // 3 floats * 4 bytes
        if(has_normal)   size += 12;  // 3 floats * 4 bytes
        if(has_tangent)  size += 12;  // 3 floats * 4 bytes
        if(has_texcoord) size += 8;   // 2 floats * 4 bytes
        if(has_color)    size += 16;  // 4 floats * 4 bytes
        
        return size;
    }

    bool VertexLayout::IsCompatible(const VertexLayout &other) const
    {
        return has_position == other.has_position
            && has_normal == other.has_normal
            && has_tangent == other.has_tangent
            && has_texcoord == other.has_texcoord
            && has_color == other.has_color;
    }
}
