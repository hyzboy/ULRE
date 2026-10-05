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
class GlobalSSBOBufferRegistry;

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

          void             SetBoundingVolumes(const BoundingVolumes &bv)
          {
              bounding_volumes=bv;

              // 一号双表：AABB 行的来源就是本包围体 ⇒ 来源一变必须刷新行。
              // （实测坑：GeometryCreater::CreateWithAABB 是"先 Create() 后 SetBoundingVolumes"，
              //   只在创建期写一次会让 AABB 行永远停在"未设"状态 —— 即始终可见哨兵。）
              RefreshGeometryAABBRow();
          }

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

    uint32_t            geometry_id = 0;
    GlobalSSBOBufferRegistry *mesh_draw_params_pool = nullptr;

public:

    /// 刷新 GeometryAABB 行（一号双表的 AABB 行**唯一写入口**）：从当前包围体生成 AABB 行。
    /// 无效包围体 ⇒ 写「始终可见」哨兵（radius < 0）；无池/无行号时静默跳过（尚未注册）。
    void                RefreshGeometryAABBRow();

    uint32_t            GetGeometryID() const { return geometry_id; }
    void                SetGeometryID(uint32_t id) { geometry_id = id; }

    bool                RegisterMeshDrawParams(GlobalSSBOBufferRegistry *pool, VulkanDevice *dev);
    bool                EnsureMeshDrawParams(GlobalSSBOBufferRegistry *pool, VulkanDevice *dev)
    {
        if (geometry_id != 0)
            return true;
        return RegisterMeshDrawParams(pool, dev);
    }

};//class Geometry
}//namespace hgl::graph
