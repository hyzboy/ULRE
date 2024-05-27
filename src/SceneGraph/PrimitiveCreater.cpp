﻿#include<hgl/graph/PrimitiveCreater.h>
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
    ibo             =nullptr;
}

bool PrimitiveCreater::Init(const AnsiString &pname,const uint32_t vertex_count,const uint32_t index_count,IndexType it)
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
        ibo=prim_data->InitIBO(index_number,index_type);

        if(!ibo)
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
                du->SetBuffer(      ibo->GetBuffer(),   prim_name+":IndexBuffer:Buffer");
                du->SetDeviceMemory(ibo->GetVkMemory(), prim_name+":IndexBuffer:Memory");
            }
        }
    #endif//_DEBUG
    }

    prim_name=pname;

    return(true);
}

const int PrimitiveCreater::GetVABIndex(const AnsiString &name,const VkFormat &acquire_format)
{
    if(!prim_data)return(-1);

    const int vab_index=prim_data->GetVABIndex(name);

    VAB *vab=prim_data->GetVAB(vab_index);

    if(!vab)
        return(-1);

    vab=prim_data->InitVAB(vab_index,acquire_format,nullptr);

    if(!vab)
        return(-1);

#ifdef _DEBUG
    if (!vdm)
    {
        DebugUtils *du=device->GetDebugUtils();

        if (du)
        {
            du->SetBuffer(vab->GetBuffer(), prim_name+":VAB:Buffer:"+name);
            du->SetDeviceMemory(vab->GetVkMemory(), prim_name+":VAB:Memory:"+name);
        }
    }
#endif//_DEBUG

    return(vab_index);
}

void *PrimitiveCreater::MapVAB(const int vab_index)
{
    VAB *vab=prim_data->GetVAB(vab_index);

    if(!vab)
        return(nullptr);

    return vab->Map(prim_data->GetVertexOffset(),vertices_number);
}

void PrimitiveCreater::UnmapVAB(const int vab_index)
{
    VAB *vab=prim_data->GetVAB(vab_index);

    if(!vab)return;

    vab->Unmap();
}

bool PrimitiveCreater::WriteVAB(const AnsiString &name,const VkFormat &format, const void *data)
{
    if(!prim_data)return(false);

    const int vab_index=GetVABIndex(name,format);

    VAB *vab=prim_data->GetVAB(vab_index);

    if(!vab)
        return(false);

    return vab->Write(data,prim_data->GetVertexOffset(),vertices_number);
}

void *PrimitiveCreater::MapIBO()
{
    if(!prim_data)return(nullptr);
    if(!ibo)return(nullptr);

    return ibo->Map(prim_data->GetFirstIndex(),index_number);
}

void PrimitiveCreater::UnmapIBO()
{
    if(ibo)
        ibo->Unmap();
}

bool PrimitiveCreater::WriteIBO(const void *data,const uint32_t count)
{
    if(!data)return(false);
    if(!prim_data)return(false);

    IndexBuffer *ibo=prim_data->GetIBO();

    if(count>0&&count>index_number)
        return(false);

   return ibo->Write(data,prim_data->GetFirstIndex(),count);
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
