#pragma once
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKVABList.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>

VK_NAMESPACE_BEGIN
class RenderAssignBuffer;
class SceneNode;
struct CameraInfo;

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

    CameraInfo *camera_info;

    RenderNodeList rn_list;

    RenderNodePointerList rn_update_l2w_list;

private:

    RenderAssignBuffer *assign_buffer;

    struct RenderItem
    {
                uint32_t                first_instance;     ///<第一个绘制实例(和instance渲染无关,对应InstanceRate的VAB)
                uint32_t                instance_count;

                MaterialInstance *      mi;

        const   MeshDataBuffer *   pdb;
        const   MeshRenderData *   prd;

    public:

        void Set(Mesh *);
    };

    IndirectDrawBuffer *icb_draw;
    IndirectDrawIndexedBuffer *icb_draw_indexed;

    void ReallocICB();
    void WriteICB(VkDrawIndirectCommand *,RenderItem *ri);
    void WriteICB(VkDrawIndexedIndirectCommand *,RenderItem *ri);

    DataArray<RenderItem> ri_array;
    uint ri_count;

    void Stat();

protected:

            VABList *               vab_list;

    const   MeshDataBuffer *        last_data_buffer;
    const   VDM *                   last_vdm;
    const   MeshRenderData *        last_render_data;

            int                     first_indirect_draw_index;
            uint                    indirect_draw_count;

    bool BindVAB(const RenderItem *);

    void ProcIndirectRender();
    bool Render(RenderItem *);

public:

    PipelineMaterialBatch(VulkanDevice *d,bool l2w,const PipelineMaterialIndex &rpi);
    ~PipelineMaterialBatch();

    void Add(MeshComponent *);

    void SetCameraInfo(CameraInfo *ci){camera_info=ci;}

    void Clear(){rn_list.Clear();}

    void End();

    void Render(RenderCmdBuffer *);

    void UpdateLocalToWorld();          //刷新所有对象的LocalToWorld矩阵
    void UpdateMaterialInstance(MeshComponent *);
};//class PipelineMaterialBatch
VK_NAMESPACE_END
