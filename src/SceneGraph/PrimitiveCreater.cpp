#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VertexDataManager.h>
#include"vulkan/VKPrimitiveData.h"

VK_NAMESPACE_BEGIN
PrimitiveCreater::PrimitiveCreater(GPUDevice *dev,const VIL *v)
{
    device          =dev;
    vdm             =nullptr;
    vil             =v;

    prim_data       =nullptr;

    Clear();
}

PrimitiveCreater::PrimitiveCreater(VertexDataManager *_vdm)
    :PrimitiveCreater(_vdm->GetDevice(),_vdm->GetVIL())
{
    vdm=_vdm;

    index_type=vdm->GetIndexType();
}

PrimitiveCreater::~PrimitiveCreater()
{
    SAFE_CLEAR(prim_data);
}

void PrimitiveCreater::Clear()
{
    SAFE_CLEAR(prim_data);

    vertices_number =0;

    index_number    =0;
    index_type      =IndexType::ERR;
    iba             =nullptr;
}

bool PrimitiveCreater::Init(const AnsiString &pname,const VkDeviceSize vertex_count,const VkDeviceSize index_count,IndexType it)
{
    if(prim_data)           //已经初始化过了
        return(false);

    if(pname.IsEmpty())return(false);
    if(vertex_count<=0)return(false);

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

        index_type=it;
        index_number=index_count;
    }

    vertices_number=vertex_count;

    if(vdm)
    {
        prim_data=CreatePrimitiveData(vdm,vertices_number);

        index_type=vdm->GetIndexType();
    }
    else
        prim_data=CreatePrimitiveData(device,vil,vertices_number);

    if(!prim_data)return(false);

    if(index_number>0)
    {
        iba=prim_data->InitIBO(index_number,index_type);

        if(!iba)
        {
            delete prim_data;
            return(false);
        }
        
    #ifdef _DEBUG
        if(!vdm)
        {
            DebugUtils *du=device->GetDebugUtils();

            if(du)
            {
                du->SetBuffer(      iba->buffer->GetBuffer(),   prim_name+":IndexBuffer:Buffer");
                du->SetDeviceMemory(iba->buffer->GetVkMemory(), prim_name+":IndexBuffer:Memory");
            }
        }
    #endif//_DEBUG
    }

    prim_name=pname;

    return(true);
}

VABAccess *PrimitiveCreater::AcquireVAB(const AnsiString &name,const VkFormat &acquire_format,const void *data)
{
    if(!prim_data)return(nullptr);
    if(name.IsEmpty())return(nullptr);

    VABAccess *vab_access=prim_data->InitVAB(name,acquire_format,data);

    if(!vab_access)
        return(nullptr);

    #ifdef _DEBUG
    if(!vdm&&vab_access->vab)
    {  
        DebugUtils *du=device->GetDebugUtils();

        if(du)
        {
            du->SetBuffer(      vab_access->vab->GetBuffer(),   prim_name+":VAB:Buffer:"+name);
            du->SetDeviceMemory(vab_access->vab->GetVkMemory(), prim_name+":VAB:Memory:"+name);
        }
    }
    #endif//_DEBUG

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

    prim_data=nullptr;      //带入Primitive后，不在这里删除

    Clear();

    return primitive;
}
VK_NAMESPACE_END
