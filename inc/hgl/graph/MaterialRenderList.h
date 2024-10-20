#pragma once
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKVABList.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>

VK_NAMESPACE_BEGIN
class RenderAssignBuffer;
class SceneNode;
struct CameraInfo;

struct RenderPipelineIndex
{
    Material *material;
    Pipeline *pipeline;

public:

    const int Comp(const RenderPipelineIndex &rli)const
    {
        if(material<rli.material)return(-1);
        if(material>rli.material)return(1);

        if(pipeline<rli.pipeline)return(-1);
        if(pipeline>rli.pipeline)return(1);

        return(0);
    }

    CompOperator(const RenderPipelineIndex &,Comp)

public:

    RenderPipelineIndex()
    {
        material=nullptr;
        pipeline=nullptr;
    }

    RenderPipelineIndex(Material *m,Pipeline *p)
    {
        material=m;
        pipeline=p;
    }
};//struct RenderPipelineIndex

/**
* 同一材质的对象渲染列表
*/
class MaterialRenderList
{
    GPUDevice *device;
    RenderCmdBuffer *cmd_buf;

    RenderPipelineIndex rp_index;

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

        const   PrimitiveDataBuffer *   pdb;
        const   PrimitiveRenderData *   prd;

    public:

        void Set(Renderable *);
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

    const   PrimitiveDataBuffer *   last_data_buffer;
    const   VDM *                   last_vdm;
    const   PrimitiveRenderData *   last_render_data;

            int                     first_indirect_draw_index=-1;
            uint                    indirect_draw_count=0;

    bool BindVAB(const PrimitiveDataBuffer *,const uint);

    void ProcIndirectRender();
    void Render(RenderItem *);

public:

    MaterialRenderList(GPUDevice *d,bool l2w,const RenderPipelineIndex &rpi);
    ~MaterialRenderList();

    void Add(SceneNode *);

    void SetCameraInfo(CameraInfo *ci)
    {
        camera_info=ci;
    }

    void Clear()
    {
        rn_list.Clear();
    }

    void End();

    void Render(RenderCmdBuffer *);

    void UpdateLocalToWorld();          //刷新所有对象的LocalToWorld矩阵
    void UpdateMaterialInstance(SceneNode *);
};//class MaterialRenderList
VK_NAMESPACE_END
