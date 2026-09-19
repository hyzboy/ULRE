#pragma once

#include<hgl/type/String.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/vk/VK.h>
#include<hgl/log/Log.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>

namespace hgl::graph{

// forward declare GeometryData to avoid including heavy headers
class GeometryData;
class DeviceBuffer;
class MeshDrawParamsPool;

#pragma pack(push, 1)
struct MeshletDescriptor
{
    uint32_t vertex_offset;    // start offset in global meshlet_vertices (unit: uint32)
    uint32_t triangle_offset;  // start offset in global meshlet_triangles (unit: u8vec3 / 3 bytes)
    uint8_t  vertex_count;     // local unique vertex count (<= 64)
    uint8_t  triangle_count;   // local micro triangle count (<= 124)
    uint16_t reserved16;       // alignment padding / flags
    uint32_t reserved32;       // pad to 16 bytes (matches BDA buffer_reference_align=16)
};
static_assert(sizeof(MeshletDescriptor) == 16, "MeshletDescriptor must be 16 bytes");

struct MeshletBounds
{
    float center[3];
    float radius;
    float cone_apex[3];
    float cone_axis[3];
    float cone_cutoff;
    int8_t cone_axis_s8[3];
    int8_t cone_cutoff_s8;
};
#pragma pack(pop)

/**
 * 几何体数据访问接口<br>
 * Geometry的存为是为了屏蔽GeometryData的初始化之类的访问接口，以便于更好的管理和使用
 */
class Geometry
{
    OBJECT_LOGGER

protected:

    AnsiString      geometry_name;

    GeometryData *  geometry_data;

protected:

    BoundingVolumes bounding_volumes;    ///<包围体

public:

    Geometry(const AnsiString &pn,GeometryData *pd);
    virtual ~Geometry();

          void             SetBoundingVolumes(const BoundingVolumes &bv){bounding_volumes=bv;}

    const BoundingVolumes &GetBoundingVolumes()const{return bounding_volumes;}

public:

    const   AnsiString &    GetName         ()const{ return geometry_name; }

    const   bool            IsValid         ()const{ return geometry_data!=nullptr; }///<是否有效

            virtual
            const   VkDeviceSize    GetVertexCount  ()const;

            const   uint32_t        GetVABCount     ()const;
            const   int             GetVABIndex     (const VertexSemantic semantic)const;

            VAB *           GetVAB          (const int)const;
            VAB *           GetVAB          (const VertexSemantic semantic)const;
            VkBuffer        GetVkBuffer     (const int index)const;
            VkBuffer        GetVkBuffer     (const VertexSemantic semantic)const;

    const   int32_t         GetVertexOffset ()const;                        ///<取得顶点偏移(注意是顶点不是字节)

    const   uint32_t        GetIndexCount   ()const;
            IndexBuffer *   GetIBO          ()const;
    const   uint32_t        GetFirstIndex   ()const;                        ///<取得第一个索引
    const   GeometryVertexFormat &GetGeometryVertexFormat()const;

    VertexDataManager *     GetVDM          ()const;                        ///<取得顶点数据管理器

protected:

    DeviceBuffer *  meshlets_buffer = nullptr;
    DeviceBuffer *  meshlet_vertices_buffer = nullptr;
    DeviceBuffer *  meshlet_triangles_buffer = nullptr;
    DeviceBuffer *  meshlet_bounds_buffer = nullptr;
    uint32_t        meshlet_count = 0;

public:

    bool            HasMeshlets() const { return meshlet_count > 0 && meshlets_buffer != nullptr; }
    uint32_t        GetMeshletCount() const { return meshlet_count; }
    DeviceBuffer *  GetMeshletsBuffer() const { return meshlets_buffer; }
    DeviceBuffer *  GetMeshletVerticesBuffer() const { return meshlet_vertices_buffer; }
    DeviceBuffer *  GetMeshletTrianglesBuffer() const { return meshlet_triangles_buffer; }
    DeviceBuffer *  GetMeshletBoundsBuffer() const { return meshlet_bounds_buffer; }

    void SetMeshlets(uint32_t count, DeviceBuffer *mb, DeviceBuffer *mvb, DeviceBuffer *mtb, DeviceBuffer *mbb = nullptr)
    {
        meshlet_count = count;
        meshlets_buffer = mb;
        meshlet_vertices_buffer = mvb;
        meshlet_triangles_buffer = mtb;
        meshlet_bounds_buffer = mbb;
    }

protected:

    uint32_t            geometry_id = 0;
    MeshDrawParamsPool *mesh_draw_params_pool = nullptr;

public:

    uint32_t            GetGeometryID() const { return geometry_id; }
    void                SetGeometryID(uint32_t id) { geometry_id = id; }

    bool                RegisterMeshDrawParams(MeshDrawParamsPool *pool, VulkanDevice *dev);
    bool                EnsureMeshDrawParams(MeshDrawParamsPool *pool, VulkanDevice *dev)
    {
        if (geometry_id != 0)
            return true;
        return RegisterMeshDrawParams(pool, dev);
    }

};//class Geometry
}//namespace hgl::graph
