#pragma once
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKVBOList.h>

VK_NAMESPACE_BEGIN
class RenderAssignBuffer;

/**
* 同一材质的对象渲染列表
*/
class MaterialRenderList
{
    GPUDevice *device;
    RenderCmdBuffer *cmd_buf;

    Material *mtl;

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

    public:

        void Set(Renderable *);
    };

    MaterialInstanceSets mi_set;
    DataArray<RenderItem> ri_array;
    uint ri_count;

    void StatMI();
    void Stat();

protected:

            VBOList *           vbo_list;

    const   VIL *               last_vil;
            Pipeline *          last_pipeline;
    const   VertexInputData *   last_vid;
            uint                last_index;

    void Bind(MaterialInstance *);
    bool Bind(const VertexInputData *,const uint);

    void Render(RenderItem *);

public:

    MaterialRenderList(GPUDevice *d,Material *m);
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
