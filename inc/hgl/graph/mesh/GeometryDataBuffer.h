#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN
/**
* 原始图元数据缓冲区<Br>
* 提供在渲染之前的数据绑定信息
*/
struct GeometryDataBuffer:public Comparator<GeometryDataBuffer>
{
    uint32_t        vab_count;
    VkBuffer *      vab_list;

    // 理论上讲，每个VAB绑定时都是可以指定byte offsets的。但是随后Draw时，又可以指定vertexOffset。
    // 在我们支持的两种draw模式中，一种是每个模型一批VAB，所有VAB的offset都是0。
    // 另一种是使用VDM，为了批量渲染，所有的VAB又必须对齐，所以每个VAB单独指定offset也不可行。

    VkDeviceSize *  vab_offset;         //注意：这里的offset是字节偏移，不是顶点偏移

    IndexBuffer *   ibo;

    VertexDataManager *vdm;             //只是用来区分和比较的，不实际使用

public:

    GeometryDataBuffer(const uint32_t,IndexBuffer *,VertexDataManager *_v=nullptr);
    ~GeometryDataBuffer();

    const int compare(const GeometryDataBuffer &mesh_data_buffer)const override;

    bool Update(const Geometry *,const VIL *);
};//struct GeometryDataBuffer
VK_NAMESPACE_END
