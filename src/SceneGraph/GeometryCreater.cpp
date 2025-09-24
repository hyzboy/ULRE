#include<hgl/graph/GeometryCreater.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKGeometry.h>
#include<hgl/graph/VertexDataManager.h>
#include"vulkan/VKGeometryData.h"

VK_NAMESPACE_BEGIN
GeometryCreater::GeometryCreater(VulkanDevice *dev,const VIL *v)
{
    device          =dev;
    vdm             =nullptr;
    vil             =v;

    has_index       =false;
    geometry_data   =nullptr;

    Clear();
}

GeometryCreater::GeometryCreater(VertexDataManager *_vdm)
    :GeometryCreater(_vdm->GetDevice(),_vdm->GetVIL())
{
    vdm=_vdm;

    has_index=vdm->GetIBO();

    index_type=vdm->GetIndexType();
}

GeometryCreater::~GeometryCreater()
{
    SAFE_CLEAR(geometry_data);
}

void GeometryCreater::Clear()
{
    SAFE_CLEAR(geometry_data);

    vertices_number =0;

    index_number    =0;
    index_type      =IndexType::ERR;
    ibo             =nullptr;
}

bool GeometryCreater::Init(const AnsiString &pname,const uint32_t vertex_count,const uint32_t index_count,IndexType it)
{
    if(geometry_data)
    {
        Clear();
        return(false);
    }

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
        geometry_data=CreateGeometryData(vdm,vertices_number);

        index_type=vdm->GetIndexType();
    }
    else
        geometry_data=CreateGeometryData(device,vil,vertices_number);

    if(!geometry_data)return(false);

    if(index_number>0)
    {
        ibo=geometry_data->InitIBO(index_number,index_type);

        if(!ibo)
        {
            delete geometry_data;
            return(false);
        }
        
    #ifdef _DEBUG
        if(!vdm)
        {
            DebugUtils *du=device->GetDebugUtils();

            if(du)
            {
                du->SetBuffer(      ibo->GetBuffer(),   geometry_name+":IndexBuffer:Buffer");
                du->SetDeviceMemory(ibo->GetVkMemory(), geometry_name+":IndexBuffer:Memory");
            }
        }
    #endif//_DEBUG
    }

    geometry_name=pname;

    return(true);
}

const int GeometryCreater::InitVAB(const AnsiString &name,const VkFormat format,const void *data)
{
    if(!geometry_data)return(-1);

    const int vab_index=geometry_data->GetVABIndex(name);

    if(vab_index<0||vab_index>=vil->GetVertexAttribCount())
        return(-1);

    if(format!=VK_FORMAT_UNDEFINED)
    {
        const VIF *vif=vil->GetConfig(vab_index);

        if(vif->format!=format)
            return(-2);
    }

    VAB *vab=geometry_data->GetVAB(vab_index);

    if(!vab)
    {
        vab=geometry_data->InitVAB(vab_index,data);

        if(!vab)
            return(-1);

    #ifdef _DEBUG
        if (!vdm)
        {
            DebugUtils *du=device->GetDebugUtils();

            if (du)
            {
                du->SetBuffer(vab->GetBuffer(), geometry_name+":VAB:Buffer:"+name);
                du->SetDeviceMemory(vab->GetVkMemory(), geometry_name+":VAB:Memory:"+name);
            }
        }
    #endif//_DEBUG
    }

    return(vab_index);
}

VABMap *GeometryCreater::GetVABMap(const AnsiString &name,const VkFormat format)
{
    const int vab_index=InitVAB(name,format,nullptr);

    if(vab_index<0)return nullptr;

    return geometry_data->GetVABMap(vab_index);
}

bool GeometryCreater::WriteVAB(const AnsiString &name,const VkFormat format,const void *data)
{
    if(!geometry_data)return(false);
    if(!data)return(false);

    return InitVAB(name,format,data)>=0;
}

IBMap *GeometryCreater::GetIBMap()
{
    if(!geometry_data)
        return(nullptr);

    return geometry_data->GetIBMap();
}

bool GeometryCreater::WriteIBO(const void *data,const uint32_t count)
{
    if(!data)return(false);
    if(!geometry_data)return(false);

    IndexBuffer *ibo=geometry_data->GetIBO();

    if(count>0&&count>index_number)
        return(false);

   return ibo->Write(data,geometry_data->GetFirstIndex(),count);
}

Geometry *GeometryCreater::Create()
{
    if(!geometry_data)
        return(nullptr);

    geometry_data->UnmapAll();

    Geometry *geometry=new Geometry(geometry_name,geometry_data);

    if(!geometry)
        return(nullptr);

    geometry_data=nullptr;      //带入Geometry后，不在这里删除

    Clear();

    return geometry;
}

// -----------------------------------------------------------------------------
//  新增：直接创建 Geometry 的便捷函数
// -----------------------------------------------------------------------------
Geometry *CreateGeometry(VulkanDevice *device, const VIL *vil, const AnsiString &name, const uint32_t vertex_count, const uint32_t index_count , IndexType it)
{
    if(!device || !vil || name.IsEmpty() || vertex_count==0)
        return nullptr;

    IndexType index_type = IndexType::ERR;

    if(index_count>0)
    {
        if(it==IndexType::AUTO)
        {
            index_type = device->ChooseIndexType(vertex_count);

            if(!IsIndexType(index_type))
                return nullptr;
        }
        else
        {
            if(!device->CheckIndexType(it, vertex_count))
                return nullptr;

            index_type = it;
        }
    }

    // 创建 GeometryData（使用 device 分配私有缓冲）
    GeometryData *pd = CreateGeometryData(device, vil, vertex_count);

    if(!pd)
        return nullptr;

    // 创建所有 VAB（内容为空）
    if(!pd->CreateAllVAB(vertex_count))
    {
        delete pd;
        return nullptr;
    }

    // 创建 IBO（如果需要）
    if(index_count>0)
    {
        IndexBuffer *ibo = pd->InitIBO(index_count, index_type);

        if(!ibo)
        {
            delete pd;
            return nullptr;
        }
    }

    // Unmap just in case
    pd->UnmapAll();

    Geometry *geometry = new Geometry(name, pd);

    if(!geometry)
    {
        delete pd;
        return nullptr;
    }

    return geometry;
}

VK_NAMESPACE_END
