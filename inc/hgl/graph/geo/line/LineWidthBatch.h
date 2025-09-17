#pragma once

#include <hgl/math/Vector.h>
#include <hgl/type/DataArray.h>
#include <hgl/graph/VKVertexAttribBuffer.h>
#include <hgl/graph/VKBuffer.h>
#include <hgl/graph/geo/line/SharedLineBackup.h>

using namespace hgl;
using namespace hgl::graph;

struct LineSegmentDescriptor
{
    Vector3f from;
    Vector3f to;

    uint8 color;
};

class LineWidthBatch
{
    VulkanDevice *      device  =nullptr;
    MaterialInstance *  mtl_inst=nullptr;
    Pipeline *          pipeline=nullptr;

    uint32      line_width  =0;

    uint32      max_count   =0;     // 当前缓冲区可容纳的最大线段数量
    uint32      count       =0;     // 当前线段数量

    Primitive * primitive   =nullptr;
    Mesh *      mesh        =nullptr;
    VABMap3f *  vab_position=nullptr;
    VABMap1u8 * vab_color   =nullptr;

    VB3f *      position    =nullptr;   // 每条线段2个顶点，每个顶点3个float
    VB1u8 *     color       =nullptr;   // 每条线段2个顶点，每个顶点4个uint8

    SharedLineBackup *shared_backup = nullptr; // optional shared backup

public:

    ~LineWidthBatch();


    void Init(const uint w,VulkanDevice *,MaterialInstance *,Pipeline *p,SharedLineBackup *sb);

    void Clear();
    bool RebuildMesh();
    void Expand(uint);

    void AddLine(const Vector3f &from,const Vector3f &to,uint8 color_index);
    void AddLine(const DataArray<LineSegmentDescriptor> &);

    void Draw(RenderCmdBuffer *);

    // expose count for manager usage
    uint32 GetCount() const { return count; }
    void SetCount(uint32 v) { count = v; }
};
