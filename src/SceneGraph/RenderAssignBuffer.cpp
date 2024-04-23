#include"RenderAssignBuffer.h"
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/mtl/UBOCommon.h>

VK_NAMESPACE_BEGIN
RenderL2WBuffer::RenderL2WBuffer(GPUDevice *dev)
{
    hgl_zero(*this);

    device=dev;
}

void RenderL2WBuffer::Clear()
{
    SAFE_CLEAR(l2w_vbo[0])
    SAFE_CLEAR(l2w_vbo[1])
    SAFE_CLEAR(l2w_vbo[2])
    SAFE_CLEAR(l2w_vbo[3])

    node_count=0;
}

#ifdef _DEBUG
namespace
{
    constexpr const char *l2w_buffer_name[]=
    {
        "VAB:Buffer:LocalToWorld:0",
        "VAB:Buffer:LocalToWorld:1",
        "VAB:Buffer:LocalToWorld:2",
        "VAB:Buffer:LocalToWorld:3"
    };

    constexpr const char *l2w_memory_name[]=
    {
        "VAB:Memory:LocalToWorld:0",
        "VAB:Memory:LocalToWorld:1",
        "VAB:Memory:LocalToWorld:2",
        "VAB:Memory:LocalToWorld:3"
    };
}
#endif//_DEBUG

void RenderL2WBuffer::Alloc(const uint nc)
{
    Clear();

    {
        node_count=nc;

        for(uint i=0;i<4;i++)
        {
            l2w_vbo[i]=device->CreateVAB(VF_V4F,node_count);
            l2w_buffer[i]=l2w_vbo[i]->GetBuffer();
        }
    }

#ifdef _DEBUG
    DebugUtils *du=device->GetDebugUtils();

    if(du)
    {
        for(int i=0;i<4;i++)
        {
            du->SetBuffer(l2w_buffer[i],l2w_buffer_name[i]);
            du->SetDeviceMemory(l2w_vbo[i]->GetVkMemory(),l2w_memory_name[i]);
        }
    }
#endif//_DEBUG
}

void RenderL2WBuffer::WriteNode(RenderNode *render_node,const uint count)
{
    RenderNode *rn;

    Alloc(count);

    glm::vec4 *tp;

    for(uint col=0;col<4;col++)
    {
        tp=(glm::vec4 *)(l2w_vbo[col]->Map());

        rn=render_node;

        for(uint i=0;i<count;i++)
        {
            *tp=rn->local_to_world[col];
            ++tp;
            ++rn;
        }

        l2w_vbo[col]->Unmap();
    }
}
VK_NAMESPACE_END

VK_NAMESPACE_BEGIN
RenderMIBuffer::RenderMIBuffer(GPUDevice *dev,const uint mi_bytes)
{
    hgl_zero(*this);

    device=dev;

    mi_data_bytes=mi_bytes;
}

void RenderMIBuffer::Bind(Material *mtl)const
{
    if(!mtl)return;

    mtl->BindUBO(DescriptorSetType::PerMaterial,mtl::SBS_MaterialInstance.name,mi_data_buffer);
}

void RenderMIBuffer::Clear()
{
    SAFE_CLEAR(mi_data_buffer);
    SAFE_CLEAR(mi_vab);

    mi_count=0;
    node_count=0;
}

void RenderMIBuffer::Alloc(const uint nc,const uint mc)
{
    Clear();

    node_count=nc;

    if(mi_data_bytes>0&&mc>0)
    {
        mi_count=mc;

        mi_data_buffer=device->CreateUBO(mi_data_bytes*mi_count);
    }

    mi_vab=device->CreateVAB(MI_VAB_FMT,node_count);
    mi_buffer=mi_vab->GetBuffer();

    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();
        
        if(du)
        {
            du->SetBuffer(mi_data_buffer->GetBuffer(),"UBO:Buffer:MaterialInstance");
            du->SetDeviceMemory(mi_data_buffer->GetVkMemory(),"UBO:Memory:MaterialInstance");

            du->SetBuffer(mi_vab->GetBuffer(),"VAB:Buffer:MaterialInstanceID");
            du->SetDeviceMemory(mi_vab->GetVkMemory(),"VAB:Memory:MaterialInstanceID");
        }
    #endif//_DEBUG
}

void RenderMIBuffer::WriteNode(RenderNode *render_node,const uint count,const MaterialInstanceSets &mi_set)
{
    RenderNode *rn;

    Alloc(count,mi_set.GetCount());

    uint8 *mip=(uint8 *)(mi_data_buffer->Map());

    for(MaterialInstance *mi:mi_set)
    {
        memcpy(mip,mi->GetMIData(),mi_data_bytes);
        mip+=mi_data_bytes;
    }

    mi_data_buffer->Unmap();

    uint16 *idp=(uint16 *)(mi_vab->Map());

    {
        rn=render_node;

        for(uint i=0;i<count;i++)
        {
            *idp=mi_set.Find(rn->ri->GetMaterialInstance());
            ++idp;

            ++rn;
        }
    }

    mi_vab->Unmap();
}
VK_NAMESPACE_END
