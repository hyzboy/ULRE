#include"RenderAssignBuffer.h"
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/mtl/UBOCommon.h>

VK_NAMESPACE_BEGIN
RenderAssignBuffer::RenderAssignBuffer(GPUDevice *dev,const uint mi_bytes)
{
    hgl_zero(*this);

    device=dev;

    mi_data_bytes=mi_bytes;

    ubo_mi=nullptr;
    mi_count=0;
}

VkBuffer RenderAssignBuffer::GetAssignVBO()const
{
    return vbo_assigns->GetBuffer();
}

void RenderAssignBuffer::Bind(MaterialInstance *mi)const
{
    const VIL *vil=mi->GetVIL();

    const uint assign_binding_count=vil->GetCount(VertexInputGroup::Assign);

    if(assign_binding_count<=0)return;

    Material *mtl=mi->GetMaterial();

    mtl->BindUBO(DescriptorSetType::PerFrame,mtl::SBS_LocalToWorld.name,ubo_l2w);
    mtl->BindUBO(DescriptorSetType::MaterialInstance,mtl::SBS_MaterialInstance.name,ubo_mi);
}

void RenderAssignBuffer::Clear()
{
    SAFE_CLEAR(ubo_l2w);
    SAFE_CLEAR(ubo_mi);
    SAFE_CLEAR(vbo_assigns);

    node_count=0;
    mi_count=0;
}

void RenderAssignBuffer::Alloc(const uint nc,const uint mc)
{
    Clear();

    {
        node_count=nc;

        ubo_l2w=device->CreateUBO(node_count*sizeof(Matrix4f));
    }

    if(mi_data_bytes>0&&mc>0)
    {
        mi_count=mc;

        ubo_mi=device->CreateUBO(mi_data_bytes*mi_count);
    }

    vbo_assigns=device->CreateVBO(ASSIGN_VBO_FMT,node_count);
}

void RenderAssignBuffer::WriteNode(RenderNode *render_node,const uint count,const MaterialInstanceSets &mi_set)
{
    RenderNode *rn;

    Alloc(count,mi_set.GetCount());

    if(ubo_mi)
    {
        uint8 *mip=(uint8 *)(ubo_mi->Map());

        for(MaterialInstance *mi:mi_set)
        {
            memcpy(mip,mi->GetMIData(),mi_data_bytes);
            mip+=mi_data_bytes;
        }

        ubo_mi->Unmap();
    }

    uint16 *idp=(uint16 *)(vbo_assigns->Map());

    {
        Matrix4f *tp=(hgl::Matrix4f *)(ubo_l2w->Map());
        Matrix4f *l2w=tp;

        rn=render_node;

        for(uint i=0;i<count;i++)
        {
            *l2w=rn->local_to_world;
            ++l2w;

            *idp=i;
            ++idp;

            *idp=mi_set.Find(rn->ri->GetMaterialInstance());
            ++idp;

            ++rn;
        }

        ubo_l2w->Unmap();
    }

    vbo_assigns->Unmap();
}
VK_NAMESPACE_END
