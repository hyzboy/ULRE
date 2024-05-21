#pragma once

#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/AABB.h>
#include<hgl/graph/VKPrimitiveData.h>

VK_NAMESPACE_BEGIN

/**
 * 单一图元数据访问接口<br>
 * 
 * 这个只是单纯的提供原始VAB/IB数据，派生为两类
 * 
 *  一类是传统的，使用独统的独立VAB的
 *  一类是使用VDM的
 * 
 * 
 * 
 * WIP: *** 1.将数据全部转移到PrimitiveData，完成旧的渲染测试
 *          2.改成抽象类，将独立VAB的做成一个实现
 *          3.实现VDM支持
 */
class Primitive
{
protected:

    AnsiString prim_name;

    PrimitiveData *prim_data;

protected:

    AABB BoundingBox;

public:

    Primitive(const AnsiString &n)
    {
        prim_name=n;
    }

    virtual ~Primitive()=0;

public:

    virtual const   VkDeviceSize    GetVertexCount  ()const=0;
    virtual const   int             GetVACount      ()const=0;
    virtual const   bool            GetVABAccess    (const AnsiString &,VABAccess *)=0;
    virtual const   IBAccess *      GetIBAccess     ()const=0;

            const   AABB &          GetBoundingBox  ()const{return BoundingBox;}
};//class Primitive

Primitive *CreatePrimitive(const PrimitiveData *);
VK_NAMESPACE_END
