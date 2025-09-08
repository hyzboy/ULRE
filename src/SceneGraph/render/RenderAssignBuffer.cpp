#include"RenderAssignBuffer.h"
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/component/MeshComponent.h>

VK_NAMESPACE_BEGIN
RenderAssignBuffer::RenderAssignBuffer(VulkanDevice *dev,Material *mtl)
{
    device=dev;

    material=mtl;

    mi_data_bytes=mtl->GetMIDataBytes();

    LW2_MAX_COUNT=dev->GetUBORange()/sizeof(Matrix4f);

    l2w_buffer_max_count=0;
    l2w_buffer=nullptr;
    mi_buffer=nullptr;

    node_count=0;
    assign_vab=nullptr;
    assign_buffer=nullptr;
}

void RenderAssignBuffer::Bind(Material *mtl)const
{
    if(!mtl)return;

    mtl->BindUBO(&mtl::SBS_LocalToWorld,     l2w_buffer);
    mtl->BindUBO(&mtl::SBS_MaterialInstance, mi_buffer);
}

void RenderAssignBuffer::Clear()
{
    SAFE_CLEAR(l2w_buffer);
    SAFE_CLEAR(mi_buffer);
    SAFE_CLEAR(assign_vab);
}

void RenderAssignBuffer::StatL2W(const RenderNodeList &rn_list)
{
    if(!l2w_buffer)
    {
        l2w_buffer_max_count=power_to_2(rn_list.GetCount());
    }
    else if(rn_list.GetCount()>l2w_buffer_max_count)
    {
        l2w_buffer_max_count=power_to_2(rn_list.GetCount());
        SAFE_CLEAR(l2w_buffer);
    }

    if(!l2w_buffer)
    {
        l2w_buffer=device->CreateUBO(sizeof(Matrix4f)*l2w_buffer_max_count);
        
    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();
        
        if(du)
        {
            du->SetBuffer(l2w_buffer->GetBuffer(),"UBO:Buffer:LocalToWorld");
            du->SetDeviceMemory(l2w_buffer->GetVkMemory(),"UBO:Memory:LocalToWorld");
        }
    #endif//_DEBUG
    }

    RenderNode *rn=rn_list.GetData();
    Matrix4f *l2wp=(Matrix4f *)(l2w_buffer->DeviceBuffer::Map());

    for(int i=0;i<rn_list.GetCount();i++)
    {
        *l2wp=rn->sm_component->GetLocalToWorldMatrix();
        ++l2wp;
        ++rn;
    }

    l2w_buffer->Unmap();
}

void RenderAssignBuffer::UpdateLocalToWorld(const RenderNodePointerList &rnp_list,const int first,const int last)
{
    if(!l2w_buffer)
        return;

    if(rnp_list.IsEmpty())
        return;

    const uint count=rnp_list.GetCount();

    RenderNode **rn=rnp_list.GetData();
    Matrix4f *l2wp=(Matrix4f *)(l2w_buffer->DeviceBuffer::Map(  sizeof(Matrix4f)*first,
                                                                sizeof(Matrix4f)*(last-first+1)));

    for(uint i=0;i<count;i++)
    {
        l2wp[(*rn)->l2w_index-first]=(*rn)->sm_component->GetLocalToWorldMatrix();

        ++rn;
    }

    l2w_buffer->Unmap();
}

void RenderAssignBuffer::UpdateMaterialInstance(const RenderNode *rn)
{
    if(!rn)
        return;

    AssignData *adp=(AssignData *)(assign_vab->DeviceBuffer::Map(sizeof(AssignData)*rn->index,sizeof(AssignData)));

    adp->mi=mi_set.Find(rn->sm_component->GetMaterialInstance());

    assign_vab->Unmap();
}

void RenderAssignBuffer::StatMI(const RenderNodeList &rn_list)
{
    mi_set.Clear();

    if(mi_data_bytes<=0)        //没有材质实例数据
        return;
    
    if(!mi_buffer)
    {
        mi_set.Reserve(power_to_2(rn_list.GetCount()));
    }
    else if(rn_list.GetCount()>mi_set.GetAllocCount())
    {
        mi_set.Reserve(power_to_2(rn_list.GetCount()));
        SAFE_CLEAR(mi_buffer);
    }

    if(!mi_buffer)
    {
        mi_buffer=device->CreateUBO(mi_data_bytes*mi_set.GetAllocCount());
        
    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();
        
        if(du)
        {
            du->SetBuffer(mi_buffer->GetBuffer(),"UBO:Buffer:MaterialInstanceData");
            du->SetDeviceMemory(mi_buffer->GetVkMemory(),"UBO:Memory:MaterialInstanceData");
        }
    #endif//_DEBUG
    }

    mi_set.Reserve(rn_list.GetCount());

    for(RenderNode &rn:rn_list)
        mi_set.Add(rn.sm_component->GetMaterialInstance());

    if(mi_set.GetCount()>material->GetMIMaxCount())
    {
        //超出最大数量了怎么办？？？
    }

    //合并材质实例数据
    {
        uint8 *mip=(uint8 *)(mi_buffer->Map());

        for(MaterialInstance *mi:mi_set)
        {
            memcpy(mip,mi->GetMIData(),mi_data_bytes);
            mip+=mi_data_bytes;
        }

        mi_buffer->Unmap();
    }
}

void RenderAssignBuffer::WriteNode(const RenderNodeList &rn_list)
{
    if(rn_list.GetCount()<=0)
        return;

    StatL2W(rn_list);
    StatMI(rn_list);

    {
        if(!assign_vab)
        {
            node_count=power_to_2(rn_list.GetCount());
        }
        else if(node_count<rn_list.GetCount())
        {
            node_count=power_to_2(rn_list.GetCount());
            SAFE_CLEAR(assign_vab);
        }

        if(!assign_vab)
        {
            assign_vab=device->CreateVAB(ASSIGN_VAB_FMT,node_count);
            assign_buffer=assign_vab->GetBuffer();
        
        #ifdef _DEBUG
            DebugUtils *du=device->GetDebugUtils();
        
            if(du)
            {
                du->SetBuffer(assign_vab->GetBuffer(),"VAB:Buffer:AssignData");
                du->SetDeviceMemory(assign_vab->GetVkMemory(),"VAB:Memory:AssignData");
            }
        #endif//_DEBUG
        }
    }
    
    //生成材质实例ID列表
    {
        RenderNode *rn=rn_list.GetData();

        AssignData *adp=(AssignData *)(assign_vab->DeviceBuffer::Map());

        for(uint i=0;i<rn_list.GetCount();i++)
        {
            rn->l2w_index=i;

            adp->l2w=i;
            adp->mi=mi_set.Find(rn->sm_component->GetMaterialInstance());
            ++adp;

            ++rn;
        }

        assign_vab->Unmap();
    }
}
VK_NAMESPACE_END
