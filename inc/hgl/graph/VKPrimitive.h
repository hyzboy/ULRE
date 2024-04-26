#ifndef HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE
#define HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE

#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/math/Math.h>
#include<hgl/graph/VKPrimitiveData.h>
VK_NAMESPACE_BEGIN
/**
 * 单一图元数据
 */
class Primitive
{
    GPUDevice *device;
    AnsiString prim_name;

protected:

    uint32_t vertex_count;

    VABAccessMap buffer_list;

    IBAccess ib_access;

    AABB BoundingBox;

protected:

    friend class RenderableNode;

public:

    Primitive(GPUDevice *dev,const AnsiString &n,const uint32_t vc=0)
    {
        device=dev;
        prim_name=n;
        vertex_count=vc;
    }

    virtual ~Primitive()=default;

            void    SetBoundingBox(const AABB &aabb){BoundingBox=aabb;}
    const   AABB &  GetBoundingBox()const           {return BoundingBox;}

            bool    Set(const AnsiString &name,VAB *vb,VkDeviceSize offset=0);

            bool    Set(IndexBuffer *ib,VkDeviceSize offset=0);

public:

    const   uint32_t            GetVertexCount      ()const {return vertex_count;}

            bool                GetVABAccess        (const AnsiString &,VABAccess *);
    const   int                 GetBufferCount      ()const {return buffer_list.GetCount();}

    const   IBAccess *          GetIBAccess         ()const {return &ib_access;}
};//class Primitive
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE
