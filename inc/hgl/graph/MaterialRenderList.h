#pragma once
#include<hgl/graph/RenderNode.h>

VK_NAMESPACE_BEGIN
struct RenderExtraBuffer;

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

    RenderExtraBuffer *extra_buffer;

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
    List<RenderItem> ri_list;
    uint ri_count;

    void Stat();

protected:

    uint32_t binding_count;
    VkBuffer *buffer_list;
    VkDeviceSize *buffer_offset;

            MaterialInstance *  last_mi;
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

    void ClearData()
    {
        rn_list.ClearData();
    }

    void End();

    void Render(RenderCmdBuffer *);
};//class MaterialRenderList
VK_NAMESPACE_END
