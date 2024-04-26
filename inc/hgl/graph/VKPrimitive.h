﻿#ifndef HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE
#define HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE

#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/AABB.h>
#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

/**
 * 单一图元数据
 */
class Primitive
{
    GPUDevice *device;
    AnsiString prim_name;

protected:

    VkDeviceSize vertex_count;

    VABAccessMap buffer_list;

    IBAccess ib_access;

    AABB BoundingBox;

protected:

    bool SetVAB(const AnsiString &name,VAB *vb,VkDeviceSize start=0);

    bool SetIndex(IndexBuffer *ib,VkDeviceSize start,const VkDeviceSize index_count);

    void SetBoundingBox(const AABB &aabb){BoundingBox=aabb;}

    friend class PrimitiveCreater;
    friend class RenderablePrimitiveCreater;

public:

    Primitive(GPUDevice *dev,const AnsiString &n,const VkDeviceSize vc=0)
    {
        device=dev;
        prim_name=n;
        vertex_count=vc;
    }

    virtual ~Primitive()=default;

public:

    const   VkDeviceSize        GetVertexCount  ()const{return vertex_count;}
    const   int                 GetVACount      ()const{return buffer_list.GetCount();}
    const   bool                GetVABAccess    (const AnsiString &,VABAccess *);
    const   IBAccess *          GetIBAccess     ()const{return ib_access.buffer?&ib_access:nullptr;}

    const   AABB &              GetBoundingBox  ()const{return BoundingBox;}
};//class Primitive
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE
