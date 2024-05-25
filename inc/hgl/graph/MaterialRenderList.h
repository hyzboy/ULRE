#pragma once
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKVABList.h>

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
                uint32_t            first;
                uint32_t            count;

                Pipeline *          pipeline;
                MaterialInstance *  mi;
        const   VertexInputData *   vid;
        const   DrawData *          dd;

    public:

        void Set(Renderable *);
    };

    DataArray<RenderItem> ri_array;
    uint ri_count;

    void Stat();

protected:

            VABList *           vbo_list;

            Pipeline *          last_pipeline;
    const   VertexInputData *   last_vid;
    const   DrawData *          last_dd;
            uint                last_index;

    bool BindVAB(const VertexInputData *,const DrawData *,const uint);

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
