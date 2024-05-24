#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VertexDataManager.h>
#include"vulkan/VKPrimitiveData.h"

VK_NAMESPACE_BEGIN
PrimitiveCreater::PrimitiveCreater(GPUDevice *dev,const VIL *v,const AnsiString &name)
{
    device          =dev;
    vdm             =nullptr;
    vil             =v;

    prim_name       =name;
    prim_data       =nullptr;

    vertices_number =0;

    index_number    =0;
    index_type      =IndexType::ERR;
    iba             =nullptr;
}

PrimitiveCreater::PrimitiveCreater(VertexDataManager *_vdm,const VIL *v,const AnsiString &name)
    :PrimitiveCreater(_vdm->GetDevice(),v,name)
{
    vdm=_vdm;
}

PrimitiveCreater::~PrimitiveCreater()
{
    SAFE_CLEAR(prim_data);
}

bool PrimitiveCreater::Init(const VkDeviceSize vertex_count,const VkDeviceSize index_count,IndexType it)
{
    if(vertex_count<=0)return(false);

    if(vdm)
        prim_data=CreatePrimitiveData(vdm,vil,vertex_count);
    else
        prim_data=CreatePrimitiveData(device,vil,vertex_count);

    if(!prim_data)return(false);

    vertices_number=vertex_count;

    if(index_count>0)
    {
        if(it==IndexType::AUTO)
        {
            it=device->ChooseIndexType(vertex_count);

            if(!IsIndexType(it))
                return(false);
        }
        else
        {
            if(!device->CheckIndexType(it,vertex_count))
                return(false);
        }

        iba=prim_data->InitIBO(index_count,it);

        if(!iba)
            return(false);

        index_type=it;
        index_number=index_count;
        
    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();

        if(du)
        {
            du->SetBuffer(      iba->buffer->GetBuffer(),   prim_name+":IndexBuffer:Buffer");
            du->SetDeviceMemory(iba->buffer->GetVkMemory(), prim_name+":IndexBuffer:Memory");
        }
    #endif//_DEBUG
    }

    return(true);
}

VABAccess *PrimitiveCreater::AcquireVAB(const AnsiString &name,const VkFormat &acquire_format,const void *data,const VkDeviceSize bytes)
{
    if(!prim_data)return(nullptr);
    if(name.IsEmpty())return(nullptr);

    VABAccess *vab_access=prim_data->InitVAB(name,acquire_format,data,bytes);

    if(!vab_access)
        return(nullptr);

    if(vab_access->vab)
    {  
    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();

        if(du)
        {
            du->SetBuffer(      vab_access->vab->GetBuffer(),   prim_name+":VAB:Buffer:"+name);
            du->SetDeviceMemory(vab_access->vab->GetVkMemory(), prim_name+":VAB:Memory:"+name);
        }
    #endif//_DEBUG
    }

    return vab_access;
}


void *PrimitiveCreater::MapIBO()
{
    if(!prim_data)return(nullptr);
    if(!iba)return(nullptr);

    return iba->buffer->Map(iba->start,iba->count);
}

void PrimitiveCreater::UnmapIBO()
{
    if(iba)
        iba->buffer->Unmap();
}

bool PrimitiveCreater::WriteIBO(const void *data,const VkDeviceSize count)
{
    if(!data)return(false);
    if(!prim_data)return(false);

    IBAccess *iba=prim_data->GetIBAccess();

    if(count>0&&count>index_number)
        return(false);

   return iba->buffer->Write(data,iba->start,count);
}

Primitive *PrimitiveCreater::Create()
{
    if(!prim_data)
        return(nullptr);

    Primitive *primitive=new Primitive(prim_name,prim_data);

    if(!primitive)
        return(nullptr);

    prim_data=nullptr;
    iba=nullptr;

    return primitive;
}
VK_NAMESPACE_END
