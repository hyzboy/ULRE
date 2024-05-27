#pragma once

#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/AABB.h>
#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN
/**
 * 原始图元数据访问接口<br>
 * Primitive的存为是为了屏蔽PrimitiveData的初始化之类的访问接口，以便于更好的管理和使用
 */
class Primitive
{
protected:

    AnsiString      prim_name;

    PrimitiveData * prim_data;

protected:

    AABB BoundingBox;
    
public:

    Primitive(const AnsiString &pn,PrimitiveData *pd);
    virtual ~Primitive();

public:

    const   AnsiString &    GetName         ()const{ return prim_name; }

    const   VkDeviceSize    GetVertexCount  ()const;
    const   int             GetVABCount     ()const;

            VABAccess *     GetVABAccess    (const AnsiString &);

            IndexBuffer *   GetIBO          ();

    const   AABB &          GetBoundingBox  ()const{return BoundingBox;}
};//class Primitive
VK_NAMESPACE_END
