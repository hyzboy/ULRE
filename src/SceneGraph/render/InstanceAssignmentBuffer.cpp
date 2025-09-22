#include"InstanceAssignmentBuffer.h"
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/DrawNode.h>
#include<hgl/graph/mesh/SubMesh.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/component/MeshComponent.h>

VK_NAMESPACE_BEGIN
InstanceAssignmentBuffer::InstanceAssignmentBuffer(VulkanDevice *dev,Material *mtl)
{
    device=dev;

    material=mtl;

    material_instance_data_bytes=mtl->GetMIDataBytes();

    MaxTransformCount=dev->GetUBORange()/sizeof(Matrix4f);

    transform_buffer_max_count=0;
    transform_buffer=nullptr;
    material_instance_buffer=nullptr;

    node_count=0;
    assign_vab=nullptr;
    assign_buffer=nullptr;
}

void InstanceAssignmentBuffer::Bind(Material *mtl)const
{
    if(!mtl)return;

    mtl->BindUBO(&mtl::SBS_LocalToWorld,     transform_buffer);
    mtl->BindUBO(&mtl::SBS_MaterialInstance, material_instance_buffer);
}

void InstanceAssignmentBuffer::Clear()
{
    SAFE_CLEAR(transform_buffer);
    SAFE_CLEAR(material_instance_buffer);
    SAFE_CLEAR(assign_vab);
}

void InstanceAssignmentBuffer::StatTransform(const DrawNodeList &draw_nodes)
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
        transform_buffer=device->CreateUBO(sizeof(Matrix4f)*transform_buffer_max_count);
        
    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();
        
        if(du)
        {
            du->SetBuffer(transform_buffer->GetBuffer(),"UBO:Buffer:LocalToWorld");
            du->SetDeviceMemory(transform_buffer->GetVkMemory(),"UBO:Memory:LocalToWorld");
        }
    #endif//_DEBUG
    }

    DrawNode *rn=draw_nodes.GetData();
    Matrix4f *l2wp=(Matrix4f *)(transform_buffer->DeviceBuffer::Map());

    for(int i=0;i<draw_nodes.GetCount();i++)
    {
        *l2wp=rn->mesh_component->GetLocalToWorldMatrix();
        ++l2wp;
        ++rn;
    }

    transform_buffer->Unmap();
}

void InstanceAssignmentBuffer::UpdateTransformData(const DrawNodePointerList &rnp_list,const int first,const int last)
{
    if(!transform_buffer)
        return;

    if(rnp_list.IsEmpty())
        return;

    const uint count=rnp_list.GetCount();

    DrawNode **rn=rnp_list.GetData();
    Matrix4f *l2wp=(Matrix4f *)(transform_buffer->DeviceBuffer::Map(  sizeof(Matrix4f)*first,
                                                                sizeof(Matrix4f)*(last-first+1)));

    for(uint i=0;i<count;i++)
    {
        l2wp[(*rn)->transform_index-first]=(*rn)->mesh_component->GetLocalToWorldMatrix();

        ++rn;
    }

    transform_buffer->Unmap();
}

void InstanceAssignmentBuffer::UpdateMaterialInstanceData(const DrawNode *rn)
{
    if(!rn)
        return;

    AssignData *adp=(AssignData *)(assign_vab->DeviceBuffer::Map(sizeof(AssignData)*rn->index,sizeof(AssignData)));

    adp->material_instance=mi_set.Find(rn->mesh_component->GetMaterialInstance());

    assign_vab->Unmap();
}

void InstanceAssignmentBuffer::StatMaterialInstance(const DrawNodeList &draw_nodes)
{
    mi_set.Clear();

    if(material_instance_data_bytes<=0)        //没有材质实例数据
        return;
    
    if(!material_instance_buffer)
    {
        mi_set.Reserve(power_to_2(draw_nodes.GetCount()));
    }
    else if(draw_nodes.GetCount()>mi_set.GetAllocCount())
    {
        mi_set.Reserve(power_to_2(draw_nodes.GetCount()));
        SAFE_CLEAR(material_instance_buffer);
    }

    if(!material_instance_buffer)
    {
        material_instance_buffer=device->CreateUBO(material_instance_data_bytes*mi_set.GetAllocCount());
        
    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();
        
        if(du)
        {
            du->SetBuffer(material_instance_buffer->GetBuffer(),"UBO:Buffer:MaterialInstanceData");
            du->SetDeviceMemory(material_instance_buffer->GetVkMemory(),"UBO:Memory:MaterialInstanceData");
        }
    #endif//_DEBUG
    }

    mi_set.Reserve(draw_nodes.GetCount());

    for(DrawNode &rn:draw_nodes)
        mi_set.Add(rn.mesh_component->GetMaterialInstance());

    if(mi_set.GetCount()>material->GetMIMaxCount())
    {
        //超出最大数量了怎么办？？？
    }

    //合并材质实例数据
    {
        uint8 *mip=(uint8 *)(material_instance_buffer->Map());

        for(MaterialInstance *mi:mi_set)
        {
            memcpy(mip,mi->GetMIData(),material_instance_data_bytes);
            mip+=material_instance_data_bytes;
        }

        material_instance_buffer->Unmap();
    }
}

void InstanceAssignmentBuffer::WriteNode(const DrawNodeList &draw_nodes)
{
    if(draw_nodes.GetCount()<=0)
        return;

    StatTransform(draw_nodes);
    StatMaterialInstance(draw_nodes);

    {
        if(!assign_vab)
        {
            node_count=power_to_2(draw_nodes.GetCount());
        }
        else if(node_count<draw_nodes.GetCount())
        {
            node_count=power_to_2(draw_nodes.GetCount());
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
        DrawNode *rn=draw_nodes.GetData();

        AssignData *adp=(AssignData *)(assign_vab->DeviceBuffer::Map());

        for(uint i=0;i<draw_nodes.GetCount();i++)
        {
            rn->transform_index=i;

            adp->transform=i;
            adp->material_instance=mi_set.Find(rn->mesh_component->GetMaterialInstance());
            ++adp;

            ++rn;
        }

        assign_vab->Unmap();
    }
}
VK_NAMESPACE_END
