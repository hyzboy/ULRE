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

    if (mesh_draw_params_pool)
    {
        if (meshlet_count != 0)
        {
            mesh_draw_params_pool->ReleaseRowRange(
                GlobalSSBOType::MeshletDescriptorPool,
                meshlet_descriptor_offset,
                meshlet_count);
            mesh_draw_params_pool->ReleaseRowRange(
                GlobalSSBOType::MeshletBoundsPool,
                meshlet_bounds_offset,
                meshlet_count);
            mesh_draw_params_pool->ReleaseRowRange(
                GlobalSSBOType::MeshletVertexPool,
                meshlet_vertex_offset,
                meshlet_vertex_count);
            mesh_draw_params_pool->ReleaseRowRange(
                GlobalSSBOType::MeshletTrianglePool,
                meshlet_triangle_offset,
                meshlet_triangle_count);
        }

        if (geometry_id != 0
         && mesh_draw_params_pool->IsActive(GlobalSSBOType::MeshDrawParams, geometry_id))
            mesh_draw_params_pool->ReleaseID(geometry_id);
    }

    geometry_id = 0;
    mesh_draw_params_pool = nullptr;

    SAFE_CLEAR(geometry_data);
}

bool Geometry::SetMeshletData(const MeshletGeometryData &data)
{
    if (geometry_id != 0 || !data.IsValid())
        return false;

    meshlet_data = data;
    return true;
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

void Geometry::RefreshGeometryAABBRow()
{
    if (!mesh_draw_params_pool || geometry_id == 0)
        return;

    const auto &src = bounding_volumes.aabb;
    mtl::GeometryAABB aabb{};

    if (src.IsValid())
    {
        const auto center = src.GetCenter();
        const auto extent = src.GetExtent();

        aabb.center[0]  = center.x;
        aabb.center[1]  = center.y;
        aabb.center[2]  = center.z;
        aabb.center[3]  = src.GetDiagonal() * 0.5f;   // 外接球半径：与 CPU 侧同式（半对角线长）
        aabb.extents[0] = extent.x;
        aabb.extents[1] = extent.y;
        aabb.extents[2] = extent.z;
    }
    else
    {
        // 无有效本地 AABB ⇒ 「始终可见」哨兵（缺数据不得导致被剔除）
        aabb.center[3] = GlobalSSBOBufferRegistry::kAlwaysVisibleRadius;
    }

    if (!mesh_draw_params_pool->WriteGeometryAABB(geometry_id,aabb))
        GLogError("[Geometry] 写 GeometryAABB 行失败：geometry_id=%u（一号双表要求与 MeshDrawParams 成对）",
                  geometry_id);
}

bool Geometry::RegisterMeshDrawParams(GlobalSSBOBufferRegistry *pool, VulkanDevice *dev)
{
    if (!pool || !dev || !geometry_data)
        return false;

    mesh_draw_params_pool = pool;
    const bool acquired_geometry_id = geometry_id == 0;

    mtl::MeshDrawParams params{};
    params.index_base = static_cast<uint32_t>(GetFirstIndex());
    params.vertex_base = static_cast<uint32_t>(GetVertexOffset());
    params.is_indexed = (GetIBO() && GetIndexCount() > 0) ? 1u : 0u;
    // 绘制元素数不再混装进一个字段：下游按 is_indexed 在 index_count / vertex_count 之间择一
    // （B3-ii；旧 total_vertices 有索引时存的是 index_count，GPU 侧无法自辨）。
    params.index_count       = static_cast<uint32_t>(GetIndexCount());
    params.vertex_count      = static_cast<uint32_t>(GetVertexCount());
    params.data_index_count  = static_cast<uint32_t>(GetIndexCount());
    params.data_vertex_count = static_cast<uint32_t>(GetVertexCount());
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

    if (geometry_id == 0)
    {
        geometry_id = pool->Acquire(params);

        if (geometry_id == 0)
            return false;
    }
    else
    {
        if (!pool->Write(geometry_id, params))
            return false;
    }

    if (!meshlet_data.descriptors.IsEmpty())
    {
        if (!meshlet_data.IsValid())
        {
            GLogError("[Geometry] Meshlet data is malformed: %s", geometry_name.c_str());
            if (acquired_geometry_id)
            {
                pool->ReleaseID(GlobalSSBOType::MeshDrawParams, geometry_id);
                geometry_id = 0;
                mesh_draw_params_pool = nullptr;
            }
            return false;
        }

        const uint32_t descriptor_count =
            static_cast<uint32_t>(meshlet_data.descriptors.GetCount());
        const uint32_t vertex_count =
            static_cast<uint32_t>(meshlet_data.vertices.GetCount());
        const uint32_t triangle_count =
            static_cast<uint32_t>(meshlet_data.triangles.GetCount());

        const uint32_t descriptor_offset =
            pool->AcquireRowRange(GlobalSSBOType::MeshletDescriptorPool, descriptor_count);
        const uint32_t bounds_offset =
            descriptor_offset == uint32_t(-1) ? uint32_t(-1)
            : pool->AcquireRowRange(GlobalSSBOType::MeshletBoundsPool, descriptor_count);
        const uint32_t vertex_offset =
            bounds_offset == uint32_t(-1) ? uint32_t(-1)
            : pool->AcquireRowRange(GlobalSSBOType::MeshletVertexPool, vertex_count);
        const uint32_t triangle_offset =
            vertex_offset == uint32_t(-1) ? uint32_t(-1)
            : pool->AcquireRowRange(GlobalSSBOType::MeshletTrianglePool, triangle_count);

        if (descriptor_offset == uint32_t(-1)
            || bounds_offset == uint32_t(-1)
            || vertex_offset == uint32_t(-1)
            || triangle_offset == uint32_t(-1))
        {
            if (descriptor_offset != uint32_t(-1))
                pool->ReleaseRowRange(GlobalSSBOType::MeshletDescriptorPool, descriptor_offset, descriptor_count);
            if (bounds_offset != uint32_t(-1))
                pool->ReleaseRowRange(GlobalSSBOType::MeshletBoundsPool, bounds_offset, descriptor_count);
            if (vertex_offset != uint32_t(-1))
                pool->ReleaseRowRange(GlobalSSBOType::MeshletVertexPool, vertex_offset, vertex_count);
            if (triangle_offset != uint32_t(-1))
                pool->ReleaseRowRange(GlobalSSBOType::MeshletTrianglePool, triangle_offset, triangle_count);
            if (acquired_geometry_id)
            {
                pool->ReleaseID(GlobalSSBOType::MeshDrawParams, geometry_id);
                geometry_id = 0;
                mesh_draw_params_pool = nullptr;
            }
            GLogError("[Geometry] Meshlet pool capacity exhausted: %s", geometry_name.c_str());
            return false;
        }

        hgl::ValueArray<ssbo::MeshletDescriptorRow> upload_descriptors;
        upload_descriptors.Resize(static_cast<int>(descriptor_count));
        bool valid_offsets = true;
        for (uint32_t i = 0; i < descriptor_count; ++i)
        {
            const ssbo::MeshletDescriptorRow &source =
                meshlet_data.descriptors[static_cast<int>(i)];
            const uint64_t global_vertex_offset =
                uint64_t(vertex_offset) + source.vertex_offset;
            const uint64_t global_triangle_offset =
                uint64_t(triangle_offset) + source.triangle_offset;
            const uint64_t global_bounds_index =
                uint64_t(bounds_offset) + source.bounds_index;
            if (global_vertex_offset > 0xFFFFFFFFu
                || global_triangle_offset > 0xFFFFFFFFu
                || global_bounds_index > 0xFFFFFFFFu)
            {
                valid_offsets = false;
                break;
            }

            upload_descriptors[static_cast<int>(i)] = source;
            upload_descriptors[static_cast<int>(i)].vertex_offset =
                static_cast<uint32_t>(global_vertex_offset);
            upload_descriptors[static_cast<int>(i)].triangle_offset =
                static_cast<uint32_t>(global_triangle_offset);
            upload_descriptors[static_cast<int>(i)].bounds_index =
                static_cast<uint32_t>(global_bounds_index);
        }

        const bool uploaded = valid_offsets
            && pool->WriteRowRange(GlobalSSBOType::MeshletDescriptorPool,
                                   descriptor_offset, upload_descriptors.GetData(), descriptor_count)
            && pool->WriteRowRange(GlobalSSBOType::MeshletBoundsPool,
                                   bounds_offset, meshlet_data.bounds.GetData(), descriptor_count)
            && pool->WriteRowRange(GlobalSSBOType::MeshletVertexPool,
                                   vertex_offset, meshlet_data.vertices.GetData(), vertex_count)
            && pool->WriteRowRange(GlobalSSBOType::MeshletTrianglePool,
                                   triangle_offset, meshlet_data.triangles.GetData(), triangle_count);

        if (!uploaded)
        {
            pool->ReleaseRowRange(GlobalSSBOType::MeshletDescriptorPool, descriptor_offset, descriptor_count);
            pool->ReleaseRowRange(GlobalSSBOType::MeshletBoundsPool, bounds_offset, descriptor_count);
            pool->ReleaseRowRange(GlobalSSBOType::MeshletVertexPool, vertex_offset, vertex_count);
            pool->ReleaseRowRange(GlobalSSBOType::MeshletTrianglePool, triangle_offset, triangle_count);
            if (acquired_geometry_id)
            {
                pool->ReleaseID(GlobalSSBOType::MeshDrawParams, geometry_id);
                geometry_id = 0;
                mesh_draw_params_pool = nullptr;
            }
            GLogError("[Geometry] Meshlet pool upload failed: %s", geometry_name.c_str());
            return false;
        }

        meshlet_descriptor_offset = descriptor_offset;
        meshlet_bounds_offset = bounds_offset;
        meshlet_vertex_offset = vertex_offset;
        meshlet_triangle_offset = triangle_offset;
        meshlet_count = descriptor_count;
        meshlet_vertex_count = vertex_count;
        meshlet_triangle_count = triangle_count;
        meshlet_data.descriptors.ClearAndDeallocate();
        meshlet_data.bounds.ClearAndDeallocate();
        meshlet_data.vertices.ClearAndDeallocate();
        meshlet_data.triangles.ClearAndDeallocate();
    }

    // ── 一号双表：AABB 行与 MeshDrawParams 行同 ID 索引，经唯一写入口刷新 ──
    // （本函数只负责"行已就位"；AABB 的**来源变化**由 SetBoundingVolumes 再刷一次 ——
    //   实测：CreateWithAABB 是"先 Create() 后 SetBoundingVolumes"，只写一次会停在哨兵。）
    RefreshGeometryAABBRow();

    return true;
}

}//namespace hgl::graph
