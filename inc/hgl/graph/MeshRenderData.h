#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN
/**
* 原始图元渲染数据<Br>
* 提供在渲染时的数据
*/
struct MeshRenderData:public ComparatorData<MeshRenderData>
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

    void Set(const Primitive *);
};
VK_NAMESPACE_END
