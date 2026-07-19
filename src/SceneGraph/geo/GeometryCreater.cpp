#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/graph/geo/VKGeometryData.h>
#include<hgl/graph/geo/GeometryVertexFormatBridge.h>
#include<hgl/graph/module/BufferManager.h>

namespace hgl::graph{
GeometryCreater::GeometryCreater(VulkanDevice *dev,const GeometryVertexFormat &gvf,BufferManager *bm)
{
    device          =dev;
    buffer_manager  =bm;
    vdm             =nullptr;
    geometry_vertex_format=gvf;

    has_index       =false;
    geometry_data   =nullptr;

    Clear();
}

GeometryCreater::GeometryCreater(VertexDataManager *_vdm)
    :GeometryCreater(_vdm->GetDevice(),_vdm->GetGeometryVertexFormat(),_vdm->GetBufferManager())
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
    if(geometry_vertex_format.GetCount()==0)return(false);

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
        geometry_data=CreateGeometryData(vdm,geometry_vertex_format,vertices_number);

        index_type=vdm->GetIndexType();
    }
    else
    {
        if (buffer_manager)
            geometry_data=CreateGeometryData(buffer_manager,geometry_vertex_format,vertices_number,buffer_policy);
        else
            geometry_data=CreateGeometryData(device,geometry_vertex_format,vertices_number,buffer_policy);
    }

    if(!geometry_data)return(false);

    if(index_number>0)
    {
        ibo=geometry_data->InitIBO(index_number,index_type,geometry_name+":IBO");

        if(!ibo)
        {
            delete geometry_data;
            geometry_data=nullptr;
            return(false);
        }
    }

    geometry_name=pname;

    return(true);
}

const int GeometryCreater::InitVAB(const VertexSemantic semantic,const VkFormat format,const void *data)
{
    if(!geometry_data)return(-1);

    const int vab_index=geometry_data->GetVABIndex(semantic);

    if(vab_index<0||vab_index>=int(geometry_data->GetVABCount()))
        return(-1);

    if(format!=VK_FORMAT_UNDEFINED)
    {
        const GeometryVertexAttributeFormat *attribute=geometry_data->GetGeometryVertexFormat().Find(semantic);
        if(!attribute||attribute->format!=format)
            return(-2);
    }

    VAB *vab=geometry_data->GetVAB(vab_index);

    if(!vab)
    {
        AnsiString vab_name = geometry_name + ":" + GetVertexSemanticName(semantic);
        vab=geometry_data->InitVAB(vab_index,data,vab_name);

        if(!vab)
            return(-1);
    }

    return(vab_index);
}

VertexAttribBuffer *GeometryCreater::GetVAB(const VertexSemantic semantic,const VkFormat format)
{
    const int vab_index=InitVAB(semantic,format,nullptr);

    if(vab_index<0)return nullptr;

    return geometry_data->GetVAB(vab_index);
}

bool GeometryCreater::WriteVAB(const VertexSemantic semantic,const VkFormat format,const void *data)
{
    if(!geometry_data)return(false);
    if(!data)return(false);

    return InitVAB(semantic,format,data)>=0;
}

int32_t GeometryCreater::GetVertexOffset()const
{
    return geometry_data?geometry_data->GetVertexOffset():0;
}

IndexBuffer *GeometryCreater::GetIBO()
{
    return geometry_data?geometry_data->GetIBO():nullptr;
}

int32_t GeometryCreater::GetFirstIndex()const
{
    return geometry_data?geometry_data->GetFirstIndex():0;
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
Geometry *CreateGeometry(VulkanDevice *device, const VIL *vil, const AnsiString &name, const uint32_t vertex_count, const uint32_t index_count , IndexType it, BufferManager *bm, BufferAllocPolicy policy)
{
    if(!vil)
        return nullptr;

    return CreateGeometry(device,
                          BuildGeometryVertexFormatFromVIFList(vil->GetVIFList(),vil->GetVertexAttribCount()),
                          name,
                          vertex_count,
                          index_count,
                          it,
                          bm,
                          policy);
}

Geometry *CreateGeometry(VulkanDevice *device, const GeometryVertexFormat &geometry_vertex_format, const AnsiString &name, const uint32_t vertex_count, const uint32_t index_count , IndexType it, BufferManager *bm, BufferAllocPolicy policy)
{
    if(!device || name.IsEmpty() || vertex_count==0)
        return nullptr;

    if(geometry_vertex_format.GetCount()==0)
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
    GeometryData *pd = bm ? CreateGeometryData(bm, geometry_vertex_format, vertex_count, policy)
                          : CreateGeometryData(device, geometry_vertex_format, vertex_count, policy);

    if(!pd)
        return nullptr;

    // 创建所有 VAB（内容为空）并传递 geometry 名字以追踪来源
    if(!pd->CreateAllVAB(name))
    {
        delete pd;
        return nullptr;
    }

    // 创建 IBO（如果需要）
    if(index_count>0)
    {
        IndexBuffer *ibo = pd->InitIBO(index_count, index_type, name+":IBO");

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

Geometry* GeometryCreater::CreateWithAABB(const math::Vector3f& min_bounds, const math::Vector3f& max_bounds)
{
    Geometry *p = Create();
    if(!p)
        return nullptr;

    math::BoundingVolumes bv;
    bv.SetFromAABB(min_bounds, max_bounds);
    p->SetBoundingVolumes(bv);

    return p;
}

}//namespace hgl::graph
