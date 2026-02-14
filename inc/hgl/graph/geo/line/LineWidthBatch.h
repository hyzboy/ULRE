#pragma once

#include <hgl/math/Vector.h>
#include <hgl/vk/VKVertexAttribBuffer.h>
#include <hgl/vk/VKBuffer.h>
#include <hgl/vk/VKBufferAccessor.h>
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

    Geometry *  geometry    =nullptr;
    Primitive * primitive   =nullptr;

    // 使用统一的BufferAccessor，适用于所有buffer类型
    BufferAccessor3f  position;   // 位置数据访问器 / Position data accessor
    BufferAccessor1u8 color;      // 颜色数据访问器 / Color data accessor

    SharedLineBackup *shared_backup = nullptr; // optional shared backup

public:

    ~LineWidthBatch();


    void Init(const uint w,VulkanDevice *,MaterialInstance *,Pipeline *p,SharedLineBackup *sb);

    void Clear();
    bool RebuildMesh();
    void Expand(uint);

    void AddLine(const Vector3f &from,const Vector3f &to,uint8 color_index);
    void AddLine(const std::vector<LineSegmentDescriptor> &);

    void Draw(RenderCmdBuffer *);

    // expose count for manager usage
    uint32 GetCount() const { return count; }
    void SetCount(uint32 v) { count = v; }

    // 新增：更新 Pipeline（RenderTarget 改变后重建的 Pipeline）
    void UpdatePipeline(Pipeline *p);
};
