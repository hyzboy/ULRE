﻿#pragma once
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKVABList.h>
#include<hgl/type/SortedSets.h>

VK_NAMESPACE_BEGIN
class RenderAssignBuffer;

/**
* 同一材质的对象渲染列表
*/
class MaterialRenderList
{
    GPUDevice *device;
    RenderCmdBuffer *cmd_buf;

    Material *material;

    RenderNodeList rn_list;

private:

    RenderAssignBuffer *assign_buffer;

    struct RenderItem
    {
                uint32_t                first;
                uint32_t                instance_count;

                Pipeline *              pipeline;
                MaterialInstance *      mi;

        const   PrimitiveDataBuffer *   pdb;
        const   PrimitiveRenderData *   prd;

    public:

        void Set(Renderable *);
    };

    SortedSets<const VDM *> vdm_set;

    DataArray<RenderItem> ri_array;
    uint ri_count;

    void Stat();

protected:

            VABList *               vab_list;

            Pipeline *              last_pipeline;
    const   PrimitiveDataBuffer *   last_data_buffer;
    const   VDM *                   last_vdm;
    const   PrimitiveRenderData *   last_render_data;

    bool BindVAB(const PrimitiveDataBuffer *,const uint);

    void Render(RenderItem *);

public:

    MaterialRenderList(GPUDevice *d,bool l2w,Material *m);
    ~MaterialRenderList();

    void Add(Renderable *ri,const Matrix4f &mat);

    void Clear()
    {
        rn_list.Clear();
    }

    void End();

    void Render(RenderCmdBuffer *);
};//class MaterialRenderList
VK_NAMESPACE_END
