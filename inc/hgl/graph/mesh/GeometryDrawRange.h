#pragma once

#include<hgl/graph/VK.h>
#include<compare>

VK_NAMESPACE_BEGIN
/**
* 网格绘制范围数据
* 用于描述渲染时可绘制的顶点/索引范围，以及缓冲区中数据容量
*/
struct GeometryDrawRange
{
    //因为要VAB是流式访问，所以我们这个结构会被用做排序依据
    //也因此，把vertex_offset放在最前面

    int32_t         vertex_offset;  //注意：这里的offset是相对于vertex的，代表第几个顶点，不是字节偏移
    uint32_t        first_index;

    // draw counts: 表示实际用于绘制的数量（默认等于 data_*）
    uint32_t        vertex_count;
    uint32_t        index_count;

    // data counts: 表示缓冲区中实际包含的数据数量（buffer capacity）
    uint32_t        data_vertex_count;
    uint32_t        data_index_count;

public:

    void Set(const Geometry *);

    std::strong_ordering operator<=>(const GeometryDrawRange &other) const
    {
        if(auto cmp = vertex_offset <=> other.vertex_offset; cmp != 0)
            return cmp;
        
        if(auto cmp = first_index <=> other.first_index; cmp != 0)
            return cmp;
        
        if(auto cmp = vertex_count <=> other.vertex_count; cmp != 0)
            return cmp;
        
        if(auto cmp = index_count <=> other.index_count; cmp != 0)
            return cmp;
        
        if(auto cmp = data_vertex_count <=> other.data_vertex_count; cmp != 0)
            return cmp;
        
        return data_index_count <=> other.data_index_count;
    }

    bool operator==(const GeometryDrawRange &other) const = default;
};
VK_NAMESPACE_END
