#pragma once
#include<hgl/graph/DrawNode.h>
#include<hgl/graph/VKVABList.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>
#include<hgl/type/ArrayList.h>

VK_NAMESPACE_BEGIN
class InstanceAssignmentBuffer;
class SceneNode;
struct CameraInfo;
class RenderComponent;
class PrimitiveComponent;

struct PipelineMaterialIndex:public Comparator<PipelineMaterialIndex>
{
    Material *material;
    Pipeline *pipeline;

public:

    const int compare(const PipelineMaterialIndex &rli)const override
    {
        if(material<rli.material)return(-1);
        if(material>rli.material)return(1);

        if(pipeline<rli.pipeline)return(-1);
        if(pipeline>rli.pipeline)return(1);

        return(0);
    }

public:

    PipelineMaterialIndex()
    {
        material=nullptr;
        pipeline=nullptr;
    }

    PipelineMaterialIndex(Material *m,Pipeline *p)
    {
        material=m;
        pipeline=p;
    }
};//struct PipelineMaterialIndex

/**
* 同一材质与管线的渲染列表
*/
class PipelineMaterialBatch
{
    VulkanDevice *device;
    RenderCmdBuffer *cmd_buf;

    PipelineMaterialIndex pm_index;

    const CameraInfo *camera_info;

    DrawNodeList draw_nodes;                 // now pointer list

    DrawNodePointerList transform_dirty_nodes;   // alias is in DrawNode.h

private:

    InstanceAssignmentBuffer *assign_buffer;

    struct DrawBatch
    {
                uint32_t                first_instance;     ///<第一个绘制实例(和instance渲染无关,对应InstanceRate的VAB)
                uint32_t                instance_count;

        const   GeometryDataBuffer *    geom_data_buffer;
        const   GeometryDrawRange *     geom_draw_range;

    public:

        void Set(Primitive *);
    };

    IndirectDrawBuffer *icb_draw;
    IndirectDrawIndexedBuffer *icb_draw_indexed;

    void ReallocICB();
    void WriteICB(VkDrawIndirectCommand *,DrawBatch *);
    void WriteICB(VkDrawIndexedIndirectCommand *,DrawBatch *);

    DataArray<DrawBatch> draw_batches;
    uint draw_batches_count;

    void BuildBatches();

protected:

            VABList *               vab_list;

    const   GeometryDataBuffer *    last_data_buffer;
    const   VDM *                   last_vdm;
    const   GeometryDrawRange *     last_draw_range;

            int                     first_indirect_draw_index;
            uint                    indirect_draw_count;

    bool BindVAB(const DrawBatch *);

    void ProcIndirectRender();
    bool Draw(DrawBatch *);

public:

    PipelineMaterialBatch(VulkanDevice *d,bool l2w,const PipelineMaterialIndex &rpi);
    ~PipelineMaterialBatch();

    void Add(DrawNode *node);          // generic path for custom nodes

    void SetCameraInfo(const CameraInfo *ci){camera_info=ci;}

    void Clear(){
        for(auto *n:draw_nodes) delete n;
        draw_nodes.Clear();
    }

    void Finalize();

    void Render(RenderCmdBuffer *);

    void UpdateTransformData();          //刷新所有对象的LocalToWorld矩阵
    void UpdateMaterialInstanceData(PrimitiveComponent *);
};//class PipelineMaterialBatch
VK_NAMESPACE_END
