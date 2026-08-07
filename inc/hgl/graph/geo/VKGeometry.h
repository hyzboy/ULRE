#pragma once

#include<hgl/type/String.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/vk/VK.h>
#include<hgl/log/Log.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>

namespace hgl::graph{

// forward declare GeometryData to avoid including heavy headers
class GeometryData;

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

};//class Geometry
}//namespace hgl::graph
