#pragma once

#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/AABB.h>
#include<hgl/graph/VK.h>
#include<hgl/log/Log.h>

VK_NAMESPACE_BEGIN

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

    AABB bounding_box;
    
public:

    Geometry(const AnsiString &pn,GeometryData *pd);
    virtual ~Geometry();

    void SetBoundingBox(const AABB &bb) { bounding_box=bb; } ///<设置包围盒
    void SetBoundingBox(const Vector3f &box_min,const Vector3f &box_max)
    {
        bounding_box.SetMinMax(box_min,box_max);
    }

public:

    const   AnsiString &    GetName         ()const{ return geometry_name; }

    const   bool            IsValid         ()const{ return geometry_data!=nullptr; }///<是否有效

    virtual
    const   VkDeviceSize    GetVertexCount  ()const;

    const   uint32_t        GetVABCount     ()const;
    const   int             GetVABIndex     (const AnsiString &name)const;

            VAB *           GetVAB          (const int)const;
            VkBuffer        GetVkBuffer     (const int index)const;

            VAB *           GetVAB          (const AnsiString &name)const{return GetVAB(GetVABIndex(name));}
            VkBuffer        GetVkBuffer     (const AnsiString &name)const;

    const   int32_t         GetVertexOffset ()const;                        ///<取得顶点偏移(注意是顶点不是字节)
            VABMap *        GetVABMap       (const int);                    ///<取得VAB映射器
            VABMap *        GetVABMap       (const AnsiString &name){return GetVABMap(GetVABIndex(name));}

    const   uint32_t        GetIndexCount   ()const;
            IndexBuffer *   GetIBO          ()const;
    const   uint32_t        GetFirstIndex   ()const;                        ///<取得第一个索引
            IBMap *         GetIBMap        ();                             ///<取得索引缓冲区映射器

    VertexDataManager *     GetVDM          ()const;                        ///<取得顶点数据管理器

    const   AABB &          GetBoundingBox  ()const{return bounding_box;}
};//class Geometry
VK_NAMESPACE_END
