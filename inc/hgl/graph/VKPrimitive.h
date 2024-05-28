﻿#pragma once

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
    const   int             GetVABIndex     (const AnsiString &name)const;
            VAB *           GetVAB          (const int);            
            VAB *           GetVAB          (const AnsiString &name){return GetVAB(GetVABIndex(name));}
    const   int32_t         GetVertexOffset ()const;                        ///<取得顶点偏移(注意是顶点不是字节)

    const   uint32_t        GetIndexCount   ()const;
            IndexBuffer *   GetIBO          ();
    const   uint32_t        GetFirstIndex   ()const;                        ///<取得第一个索引

    VertexDataManager *     GetVDM          ();                             ///<取得顶点数据管理器

    const   AABB &          GetBoundingBox  ()const{return BoundingBox;}
};//class Primitive
VK_NAMESPACE_END
