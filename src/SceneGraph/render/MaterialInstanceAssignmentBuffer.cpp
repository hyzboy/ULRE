#include"MaterialInstanceAssignmentBuffer.h"
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/DrawNode.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKRenderAssign.h>
#include<hgl/graph/mtl/UBOCommon.h>

VK_NAMESPACE_BEGIN
MaterialInstanceAssignmentBuffer::MaterialInstanceAssignmentBuffer(VulkanDevice *dev,Material *mtl)
{
    device=dev;

    material=mtl;

    material_instance_data_bytes=mtl->GetMIDataBytes();

    material_instance_buffer=nullptr;

    node_count=0;
    material_instance_vab=nullptr;
    material_instance_vab_buffer=nullptr;
}

void MaterialInstanceAssignmentBuffer::BindMaterialInstance(Material *mtl)const
{
    if(!mtl)return;

    mtl->BindUBO(&mtl::SBS_MaterialInstance, material_instance_buffer);
}

void MaterialInstanceAssignmentBuffer::Clear()
{
    SAFE_CLEAR(material_instance_buffer);
    SAFE_CLEAR(material_instance_vab);
}

void MaterialInstanceAssignmentBuffer::UpdateMaterialInstanceData(const DrawNode *rn)
{
    if(!rn)
        return;

    uint16 *mip=(uint16 *)(material_instance_vab->DeviceBuffer::Map(sizeof(uint16)*rn->index,sizeof(uint16)));

    *mip=mi_set.Find(rn->GetMaterialInstance());

    material_instance_vab->Unmap();
}

void MaterialInstanceAssignmentBuffer::StatMaterialInstance(const DrawNodeList &draw_nodes)
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

    for(DrawNode *rn:draw_nodes)
        mi_set.Add(rn->GetMaterialInstance());

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

void MaterialInstanceAssignmentBuffer::WriteNode(const DrawNodeList &draw_nodes)
{
    if(draw_nodes.GetCount()<=0)
        return;

    StatMaterialInstance(draw_nodes);

    {
        if(!material_instance_vab)
        {
            node_count=power_to_2(draw_nodes.GetCount());
        }
        else if(node_count<draw_nodes.GetCount())
        {
            node_count=power_to_2(draw_nodes.GetCount());
            SAFE_CLEAR(material_instance_vab);
        }

        if(!material_instance_vab)
        {
            material_instance_vab=device->CreateVAB(VK_FORMAT_R16_UINT,node_count);
            material_instance_vab_buffer=material_instance_vab->GetBuffer();
        
        #ifdef _DEBUG
            DebugUtils *du=device->GetDebugUtils();
        
            if(du)
            {
                du->SetBuffer(material_instance_vab->GetBuffer(),"VAB:Buffer:MaterialInstanceID");
                du->SetDeviceMemory(material_instance_vab->GetVkMemory(),"VAB:Memory:MaterialInstanceID");
            }
        #endif//_DEBUG
        }
    }

    //生成材质实例ID列表
    {
        DrawNode **rn=draw_nodes.GetData();

        uint16 *mi_ptr=(uint16 *)(material_instance_vab->DeviceBuffer::Map());

        for(uint i=0;i<draw_nodes.GetCount();i++)
        {
            *mi_ptr=mi_set.Find((*rn)->GetMaterialInstance());
            ++mi_ptr;
            ++rn;
        }

        material_instance_vab->Unmap();
    }
}
VK_NAMESPACE_END
