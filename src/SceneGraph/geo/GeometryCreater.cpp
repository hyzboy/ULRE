#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/buffer/IndexBuffer.h>
#include<hgl/vk/buffer/VertexAttribBuffer.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/geo/VertexDataManager.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/graph/geo/VKGeometryData.h>
#include<hgl/graph/geo/MeshletGeometryData.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>

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
    :GeometryCreater(_vdm->GetDevice(),_vdm->GetGeometryVertexFormat(),
                     dynamic_cast<BufferManager *>(_vdm->GetAllocator()))
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
    meshlet_positions.Clear();
    meshlet_indices.Clear();
    meshlet_build_enabled = true;

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

    if (InitVAB(semantic,format,data) < 0)
        return false;

    if (semantic == VertexSemantic::Position)
    {
        meshlet_positions.Clear();
        if (format == VK_FORMAT_R32G32B32_SFLOAT)
        {
            meshlet_positions.Add(static_cast<const float *>(data),
                                  static_cast<int>(vertices_number * 3u));
        }
        else if (format == VK_FORMAT_R32G32B32A32_SFLOAT)
        {
            const float *source = static_cast<const float *>(data);
            for (uint32_t i = 0; i < vertices_number; ++i)
            {
                meshlet_positions.Add(source[i * 4u]);
                meshlet_positions.Add(source[i * 4u + 1u]);
                meshlet_positions.Add(source[i * 4u + 2u]);
            }
        }
    }

    return true;
}

void GeometryCreater::RecordMeshletPosition(const float x, const float y, const float z)
{
    meshlet_positions.Add(x);
    meshlet_positions.Add(y);
    meshlet_positions.Add(z);
}

static void CaptureMeshletSources(GeometryData *geometry_data,
                                  const uint32_t vertex_count,
                                  const uint32_t index_count,
                                  hgl::ValueArray<float> &positions,
                                  hgl::ValueArray<uint32_t> &indices)
{
    if (positions.GetCount() != static_cast<int>(vertex_count * 3u))
    {
        VAB *position_vab = geometry_data->GetVAB(VertexSemantic::Position);
        void *mapped = position_vab ? position_vab->Map(0, vertex_count) : nullptr;
        if (mapped)
        {
            positions.Clear();
            const auto *source = static_cast<const float *>(mapped);
            switch (position_vab->GetFormat())
            {
            case VK_FORMAT_R32G32B32_SFLOAT:
                positions.Add(source, static_cast<int>(vertex_count * 3u));
                break;
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                for (uint32_t i = 0; i < vertex_count; ++i)
                {
                    positions.Add(source[i * 4u]);
                    positions.Add(source[i * 4u + 1u]);
                    positions.Add(source[i * 4u + 2u]);
                }
                break;
            case VK_FORMAT_R32G32_SFLOAT:
                for (uint32_t i = 0; i < vertex_count; ++i)
                {
                    positions.Add(source[i * 2u]);
                    positions.Add(source[i * 2u + 1u]);
                    positions.Add(0.0f);
                }
                break;
            default:
                break;
            }
            position_vab->Unmap();
        }
    }

    if (indices.GetCount() != static_cast<int>(index_count))
    {
        IndexBuffer *ibo = geometry_data->GetIBO();
        void *mapped = ibo ? ibo->Map(0, index_count) : nullptr;
        if (!mapped)
            return;

        indices.Clear();
        switch (ibo->GetType())
        {
        case IndexType::U8:
        {
            const auto *source = static_cast<const uint8_t *>(mapped);
            for (uint32_t i = 0; i < index_count; ++i)
                indices.Add(source[i]);
            break;
        }
        case IndexType::U16:
        {
            const auto *source = static_cast<const uint16_t *>(mapped);
            for (uint32_t i = 0; i < index_count; ++i)
                indices.Add(source[i]);
            break;
        }
        case IndexType::U32:
        {
            const auto *source = static_cast<const uint32_t *>(mapped);
            indices.Add(source, static_cast<int>(index_count));
            break;
        }
        default:
            break;
        }
        ibo->Unmap();
    }
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
    if(count > index_number)
        return false;

    IndexBuffer *ibo=geometry_data->GetIBO();
    if (!ibo)
        return false;

    if (!ibo->Write(data, geometry_data->GetFirstIndex(), count))
        return false;

    meshlet_indices.Clear();
    meshlet_indices.Add(static_cast<const uint32_t *>(data), static_cast<int>(count));
    return true;
}

Geometry *GeometryCreater::Create()
{
    if(!geometry_data)
        return(nullptr);

    geometry_data->UnmapAll();
    if (meshlet_build_enabled && index_number >= 3 && index_number % 3 == 0)
        CaptureMeshletSources(geometry_data, vertices_number, index_number,
                              meshlet_positions, meshlet_indices);

    Geometry *geometry=new Geometry(geometry_name,geometry_data);

    if(!geometry)
        return(nullptr);

    if (meshlet_build_enabled
        && index_number >= 3
        && index_number % 3 == 0
        && meshlet_positions.GetCount() == static_cast<int>(vertices_number * 3u)
        && meshlet_indices.GetCount() == static_cast<int>(index_number))
    {
        MeshletGeometryData meshlet_data;
        if (!BuildMeshletGeometryData(meshlet_positions.GetData(),
                                      vertices_number,
                                      meshlet_indices.GetData(),
                                      index_number,
                                      meshlet_data)
            || !geometry->SetMeshletData(meshlet_data))
        {
            GLogError("[GeometryCreater] Meshlet build failed: %s", geometry_name.c_str());
            delete geometry;
            geometry_data = nullptr;
            Clear();
            return nullptr;
        }
    }
    else if (meshlet_build_enabled && index_number >= 3 && index_number % 3 == 0)
        GLogWarning("[GeometryCreater] Meshlet omitted because CPU source data is unavailable: %s",
                    geometry_name.c_str());

    auto *bm = buffer_manager;
    if (bm && bm->GetGraphicsContext())
    {
        auto *gc = bm->GetGraphicsContext();
        auto *dev = device ? device : (vdm ? vdm->GetDevice() : nullptr);
        if (gc && dev)
        {
            if (auto *pool = gc->GetGlobalSSBOBufferRegistry())
                geometry->RegisterMeshDrawParams(pool, dev);
        }
    }

    geometry_data=nullptr;      //带入Geometry后，不在这里删除

    Clear();

    return geometry;
}

// -----------------------------------------------------------------------------
//  新增：直接创建 Geometry 的便捷函数
// -----------------------------------------------------------------------------
Geometry *CreateGeometry(VulkanDevice *device, const GeometryVertexFormat &geometry_vertex_format, const AnsiString &name, const uint32_t vertex_count, const uint32_t index_count , IndexType it, BufferManager *bm, BufferAllocPolicy policy)
{
    if(!device || name.IsEmpty() || vertex_count==0)
        return nullptr;

    if(geometry_vertex_format.GetCount()==0)
        return nullptr;

    IndexType index_type = IndexType::ERR;

    if(index_count>0)
    {
        // 索引 buffer 统一使用 U32：mesh shader 的 SSBO 顶点取索按 uint[]（4 字节
        // 步长）读取 sbo_vertex_index（MeshShaderModeVertexPassthrough），U16/U8
        // buffer 会被错读出乱值索引；引擎已无 vkCmdDrawIndexed 路径，U32 化无副作用。
        it=IndexType::U32;

        if(!device->CheckIndexType(it, vertex_count))
            return nullptr;

        index_type = it;
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

    if (bm && bm->GetGraphicsContext())
    {
        auto *gc = bm->GetGraphicsContext();
        if (gc && device)
        {
            if (auto *pool = gc->GetGlobalSSBOBufferRegistry())
                geometry->RegisterMeshDrawParams(pool, device);
        }
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
