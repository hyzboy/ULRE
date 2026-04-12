#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/FormatAwareWriter.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/graph/geo/VKGeometryData.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/log/Log.h>
#include<cassert>
#include<cstdio>
#include<cstring>

namespace hgl::graph{
GeometryCreater::GeometryCreater(VulkanDevice *dev,const VertexFormatMap &format_map,BufferManager *bm)
{
    device          =dev;
    buffer_manager  =bm;
    vdm             =nullptr;
    vertex_format_map = format_map;

    has_index       =false;
    geometry_data   =nullptr;
    inline_geo_format_preset=inline_geometry::InlineGeoFormatPreset::Legacy;

    Clear();
}

GeometryCreater::GeometryCreater(VertexDataManager *_vdm)
{
    device          = _vdm ? _vdm->GetDevice() : nullptr;
    buffer_manager  = _vdm ? _vdm->GetBufferManager() : nullptr;
    vdm=_vdm;
    vertex_format_map = _vdm ? _vdm->GetVertexFormatMap() : VertexFormatMap();

    has_index=vdm ? vdm->GetIBO() : nullptr;

    index_type=vdm ? vdm->GetIndexType() : IndexType::ERR;

    geometry_data   =nullptr;
    inline_geo_format_preset=inline_geometry::InlineGeoFormatPreset::Legacy;
    Clear();
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

inline_geometry::FormatAwareWriter GeometryCreater::GetFormatAwareWriter()
{
    return inline_geometry::FormatAwareWriter(this,inline_geo_format_preset);
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
    {
        if (buffer_manager)
            geometry_data=CreateGeometryData(buffer_manager,vertex_format_map,vertices_number,buffer_policy);
        else
            geometry_data=CreateGeometryData(device,vertex_format_map,vertices_number,buffer_policy);
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

const int GeometryCreater::InitVAB(const VertexAttrib &attrib,const VkFormat format,const void *data)
{
    if(!geometry_data)
    {
        std::fprintf(stderr,
            "[GeometryCreater] InitVAB failed: geometry_data missing, attrib='%s'\n",
            GetVertexAttribName(attrib));
#ifdef _DEBUG
        assert(false && "GeometryCreater::InitVAB geometry_data missing");
#endif
        return(-1);
    }

    const int vab_index=geometry_data->GetVABIndex(attrib);

    if(vab_index<0||vab_index>=static_cast<int>(geometry_data->GetVABCount()))
    {
        std::fprintf(stderr,
            "[GeometryCreater] InitVAB failed: attrib='%s' not present in geometry layout, index=%d attrib_count=%u\n",
            GetVertexAttribName(attrib),
            vab_index,
            geometry_data ? geometry_data->GetVABCount() : 0u);
//#ifdef _DEBUG
//        assert(false && "GeometryCreater::InitVAB attrib missing in VIL");
//#endif
        return(-1);
    }

    if(format!=VK_FORMAT_UNDEFINED)
    {
        const VkFormat expected_format=geometry_data->GetVABFormat(vab_index);
        if(expected_format==VK_FORMAT_UNDEFINED)
            return(-1);

        if(expected_format!=format)
        {
            std::fprintf(stderr,
                "[GeometryCreater] InitVAB format mismatch: geom='%s' attrib='%s' expected='%s' requested='%s'\n",
                geometry_name.c_str(),
                GetVertexAttribName(attrib),
                GetVulkanFormatName(expected_format),
                GetVulkanFormatName(format));
#ifdef _DEBUG
            assert(false && "GeometryCreater::InitVAB format mismatch");
#endif
            return(-2);
        }

        const uint32_t expected_stride = GetStrideByFormat(format);
        if(expected_stride == 0)
        {
            std::fprintf(stderr,
                "[GeometryCreater] InitVAB invalid format stride: geom='%s' attrib='%s' format='%s'\n",
                geometry_name.c_str(),
                GetVertexAttribName(attrib),
                GetVulkanFormatName(format));
#ifdef _DEBUG
            assert(false && "GeometryCreater::InitVAB invalid format stride");
#endif
            return(-3);
        }
    }

    VAB *vab=geometry_data->GetVABByIndex(vab_index);

    if(!vab)
    {
        AnsiString vab_name = geometry_name + AnsiString(":") + GetVertexAttribName(attrib);
        vab=geometry_data->InitVAB(vab_index,data,vab_name);

        if(!vab)
            return(-1);

        GLogDebug("[VAB_CREATE] geom='%s' attrib=%s idx=%d VkBuffer=%p data_ptr=%p",
                  geometry_name.c_str(), GetVertexAttribName(attrib), vab_index,
                  (void*)vab->GetVkBuffer(), data);
    }

    return(vab_index);
}

VertexAttribBuffer *GeometryCreater::GetVAB(const VertexAttrib attrib,const VkFormat format)
{
    const int vab_index=InitVAB(attrib,format,nullptr);

    if(vab_index<0)
        return nullptr;

    VAB *vab = geometry_data->GetVABByIndex(vab_index);

    if(vab && format != VK_FORMAT_UNDEFINED)
    {
        const uint32_t expected_stride = GetStrideByFormat(format);
        if(vab->GetFormat() != format || vab->GetStride() != expected_stride)
        {
            std::fprintf(stderr,
                "[GeometryCreater] GetVAB validation failed: geom='%s' attrib='%s' format='%s' actual_format='%s' expected_stride=%u actual_stride=%u\n",
                geometry_name.c_str(),
                GetVertexAttribName(attrib),
                GetVulkanFormatName(format),
                GetVulkanFormatName(vab->GetFormat()),
                expected_stride,
                vab->GetStride());
#ifdef _DEBUG
            assert(false && "GeometryCreater::GetVAB validation failed");
#endif
            return nullptr;
        }
    }

    return vab;
}

bool GeometryCreater::WriteVAB(const VertexAttrib attrib,const VkFormat format,const void *data)
{
    if(!geometry_data)
    {
        std::fprintf(stderr,
            "[GeometryCreater] WriteVAB failed: geometry_data missing, attrib='%s'\n",
            GetVertexAttribName(attrib));
#ifdef _DEBUG
        assert(false && "GeometryCreater::WriteVAB geometry_data missing");
#endif
        return(false);
    }

    if(!data)
    {
        std::fprintf(stderr,
            "[GeometryCreater] WriteVAB failed: null data, geom='%s' attrib='%s'\n",
            geometry_name.c_str(),
            GetVertexAttribName(attrib));
#ifdef _DEBUG
        assert(false && "GeometryCreater::WriteVAB null data");
#endif
        return(false);
    }

    if(format == VK_FORMAT_UNDEFINED)
    {
        std::fprintf(stderr,
            "[GeometryCreater] WriteVAB failed: explicit format required, geom='%s' attrib='%s'\n",
            geometry_name.c_str(),
            GetVertexAttribName(attrib));
#ifdef _DEBUG
        assert(false && "GeometryCreater::WriteVAB explicit format required");
#endif
        return(false);
    }

    if(GetStrideByFormat(format) == 0)
    {
        std::fprintf(stderr,
            "[GeometryCreater] WriteVAB failed: invalid format stride, geom='%s' attrib='%s' format='%s'\n",
            geometry_name.c_str(),
            GetVertexAttribName(attrib),
            GetVulkanFormatName(format));
#ifdef _DEBUG
        assert(false && "GeometryCreater::WriteVAB invalid format stride");
#endif
        return(false);
    }

    return InitVAB(attrib,format,data)>=0;
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
Geometry *CreateGeometry(VulkanDevice *device, const VertexFormatMap &format_map, const AnsiString &name, const uint32_t vertex_count, const uint32_t index_count , IndexType it, BufferManager *bm, BufferAllocPolicy policy)
{
    if(!device || format_map.empty() || name.IsEmpty() || vertex_count==0)
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
    GeometryData *pd = bm ? CreateGeometryData(bm, format_map, vertex_count, policy)
                          : CreateGeometryData(device, format_map, vertex_count, policy);

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
