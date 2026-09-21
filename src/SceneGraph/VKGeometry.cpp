#include<hgl/graph/geo/VKGeometryData.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/buffer/VertexAttribBuffer.h>
#include<hgl/vk/buffer/IndexBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>

#ifdef _DEBUG
#include<hgl/vk/VKDeviceAttribute.h>
#endif//_DEBUG

namespace hgl::graph{

Geometry::Geometry(const AnsiString &pn,GeometryData *pd)
{
    geometry_name=pn;
    geometry_data=pd;

    LogVerbose(" Geometry: "+geometry_name);
}

Geometry::~Geometry()
{
    LogVerbose("~Geometry: "+geometry_name);

    if (mesh_draw_params_pool && geometry_id != 0
     && mesh_draw_params_pool->IsActive(GlobalSSBOType::MeshDrawParams, geometry_id))
    {
        mesh_draw_params_pool->ReleaseID(geometry_id);
        geometry_id = 0;
        mesh_draw_params_pool = nullptr;
    }

    SAFE_CLEAR(geometry_data);
    SAFE_CLEAR(meshlets_buffer);
    SAFE_CLEAR(meshlet_vertices_buffer);
    SAFE_CLEAR(meshlet_triangles_buffer);
    SAFE_CLEAR(meshlet_bounds_buffer);
}

const VkDeviceSize Geometry::GetVertexCount()const
{
    return geometry_data->GetVertexCount();
}

const uint32_t Geometry::GetVABCount()const
{
    return geometry_data->GetVABCount();
}

const int Geometry::GetVABIndex(const VertexSemantic semantic)const
{
    return geometry_data->GetVABIndex(semantic);
}

VAB *Geometry::GetVAB(const int vab_index)const
{
    return geometry_data->GetVAB(vab_index);
}

VAB *Geometry::GetVAB(const VertexSemantic semantic)const
{
    return geometry_data->GetVAB(semantic);
}

VkBuffer Geometry::GetVkBuffer(const int index)const
{
    VAB *vab=GetVAB(index);
    if(!vab)return(VK_NULL_HANDLE);
    return vab->GetVkBuffer();
}

VkBuffer Geometry::GetVkBuffer(const VertexSemantic semantic)const
{
    VAB *vab=GetVAB(semantic);
    if(!vab)return(VK_NULL_HANDLE);
    return vab->GetVkBuffer();
}

const int32_t Geometry::GetVertexOffset()const
{
    return geometry_data->GetVertexOffset();
}

const uint32_t Geometry::GetIndexCount()const
{
    return geometry_data->GetIndexCount();
}

const GeometryVertexFormat &Geometry::GetGeometryVertexFormat()const
{
    return geometry_data->GetGeometryVertexFormat();
}

IndexBuffer *Geometry::GetIBO()const
{
    return geometry_data->GetIBO();
}

const uint32_t Geometry::GetFirstIndex()const
{
    return geometry_data->GetFirstIndex();
}

VertexDataManager *Geometry::GetVDM()const
{
    return geometry_data->GetVDM();
}

bool Geometry::RegisterMeshDrawParams(GlobalSSBOBufferRegistry *pool, VulkanDevice *dev)
{
    if (!pool || !dev || !geometry_data)
        return false;

    mesh_draw_params_pool = pool;

    mtl::MeshDrawParams params{};
    params.index_base = static_cast<uint32_t>(GetFirstIndex());
    params.vertex_base = static_cast<uint32_t>(GetVertexOffset());
    params.is_indexed = (GetIBO() && GetIndexCount() > 0) ? 1u : 0u;
    params.total_vertices = GetIndexCount() > 0 ? GetIndexCount() : static_cast<uint32_t>(GetVertexCount());
    params.char_height = 0.0f;
    params.first_instance = 0;

    const auto &gvf = GetGeometryVertexFormat();
    for (uint32_t i = 0; i < gvf.GetCount(); ++i)
    {
        const auto *attr = gvf.Get(i);
        if (!attr || attr->semantic == VertexSemantic::Unknown)
            continue;

        const VkBuffer buf = GetVkBuffer(attr->semantic);
        if (buf == VK_NULL_HANDLE)
            continue;

        const uint64_t addr = dev->GetBufferDeviceAddressAligned16(buf);
        switch (attr->semantic)
        {
        case VertexSemantic::Position:    params.addr_position = addr; break;
        case VertexSemantic::TexCoord:    params.addr_uv = addr; break;
        case VertexSemantic::Normal:      params.addr_ntb = addr; break;
        case VertexSemantic::Color:       params.addr_color = addr; break;
        case VertexSemantic::Luminance:   params.addr_luminance = addr; break;
        case VertexSemantic::TransformID: params.addr_transform_id = addr; break;
        case VertexSemantic::Size:        params.addr_size = addr; break;
        default: break;
        }
    }

    if (auto *ibo = GetIBO())
    {
        params.addr_index = dev->GetBufferDeviceAddressAligned16(ibo->GetVkBuffer());
    }

    if (HasMeshlets())
    {
        if (auto *mb = GetMeshletsBuffer())
            params.addr_meshlets = dev->GetBufferDeviceAddressAligned16(mb->GetBuffer());
        if (auto *mvb = GetMeshletVerticesBuffer())
            params.addr_meshlet_vertices = dev->GetBufferDeviceAddressAligned16(mvb->GetBuffer());
        if (auto *mtb = GetMeshletTrianglesBuffer())
            params.addr_meshlet_triangles = dev->GetBufferDeviceAddressAligned16(mtb->GetBuffer());
    }

    if (geometry_id == 0)
    {
        geometry_id = pool->Acquire(params);
        return geometry_id != 0;
    }
    else
    {
        return pool->Write(geometry_id, params);
    }
}

}//namespace hgl::graph
