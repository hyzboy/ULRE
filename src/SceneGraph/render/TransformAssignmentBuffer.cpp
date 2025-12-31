#include"TransformAssignmentBuffer.h"
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/DrawNode.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/mtl/UBOCommon.h>

VK_NAMESPACE_BEGIN
TransformAssignmentBuffer::TransformAssignmentBuffer(VulkanDevice *dev)
{
    device=dev;

    MaxTransformCount=dev->GetUBORange()/sizeof(math::Matrix4f);

    transform_buffer_max_count=0;
    transform_buffer=nullptr;

    node_count=0;
    transform_vab=nullptr;
    transform_vab_buffer=nullptr;
}

void TransformAssignmentBuffer::BindTransform(Material *mtl)const
{
    if(!mtl)return;

    mtl->BindUBO(&mtl::SBS_LocalToWorld, transform_buffer);
}

void TransformAssignmentBuffer::Clear()
{
    SAFE_CLEAR(transform_buffer);
    SAFE_CLEAR(transform_vab);
}

void TransformAssignmentBuffer::StatTransform(const DrawNodeList &draw_nodes)
{
    if(!transform_buffer)
    {
        transform_buffer_max_count=power_to_2(draw_nodes.GetCount());
    }
    else if(draw_nodes.GetCount()>transform_buffer_max_count)
    {
        transform_buffer_max_count=power_to_2(draw_nodes.GetCount());
        SAFE_CLEAR(transform_buffer);
    }

    if(!transform_buffer)
    {
        transform_buffer=device->CreateUBO(sizeof(math::Matrix4f)*transform_buffer_max_count);
        
    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();
        
        if(du)
        {
            du->SetBuffer(transform_buffer->GetBuffer(),"UBO:Buffer:LocalToWorld");
            du->SetDeviceMemory(transform_buffer->GetVkMemory(),"UBO:Memory:LocalToWorld");
        }
    #endif//_DEBUG
    }

    DrawNode **rn=draw_nodes.GetData();
    Matrix4f *l2wp=(math::Matrix4f *)(transform_buffer->DeviceBuffer::Map());

    for(int i=0;i<draw_nodes.GetCount();i++)
    {
        auto *tf=(*rn)->GetTransform();
        *l2wp=tf?tf->GetLocalToWorldMatrix():math::Identity4f;
        ++l2wp;
        ++rn;
    }

    transform_buffer->Unmap();
}

void TransformAssignmentBuffer::UpdateTransformData(const DrawNodeList &rnp_list,const int first,const int last)
{
    if(!transform_buffer)
        return;

    if(rnp_list.IsEmpty())
        return;

    const uint count=rnp_list.GetCount();

    DrawNode **rn=rnp_list.GetData();
    Matrix4f *l2wp=(math::Matrix4f *)(transform_buffer->DeviceBuffer::Map(  sizeof(math::Matrix4f)*first,
                                                                sizeof(math::Matrix4f)*(last-first+1)));

    for(uint i=0;i<count;i++)
    {
        auto *tf=(*rn)->GetTransform();
        l2wp[(*rn)->transform_index-first]=tf?tf->GetLocalToWorldMatrix():math::Identity4f;

        ++rn;
    }

    transform_buffer->Unmap();
}

void TransformAssignmentBuffer::WriteNode(const DrawNodeList &draw_nodes)
{
    if(draw_nodes.GetCount()<=0)
        return;

    StatTransform(draw_nodes);

    {
        if(!transform_vab)
        {
            node_count=power_to_2(draw_nodes.GetCount());
        }
        else if(node_count<draw_nodes.GetCount())
        {
            node_count=power_to_2(draw_nodes.GetCount());
            SAFE_CLEAR(transform_vab);
        }

        if(!transform_vab)
        {
            transform_vab=device->CreateVAB(VK_FORMAT_R16_UINT,node_count);
            transform_vab_buffer=transform_vab->GetBuffer();
        
        #ifdef _DEBUG
            DebugUtils *du=device->GetDebugUtils();
        
            if(du)
            {
                du->SetBuffer(transform_vab->GetBuffer(),"VAB:Buffer:TransformID");
                du->SetDeviceMemory(transform_vab->GetVkMemory(),"VAB:Memory:TransformID");
            }
        #endif//_DEBUG
        }
    }
    
    //生成transform索引列表
    {
        DrawNode **rn=draw_nodes.GetData();

        uint16 *transform_ptr=(uint16 *)(transform_vab->DeviceBuffer::Map());

        for(uint i=0;i<draw_nodes.GetCount();i++)
        {
            (*rn)->transform_index=i;
            *transform_ptr=i;
            ++transform_ptr;
            ++rn;
        }

        transform_vab->Unmap();
    }
}
VK_NAMESPACE_END
