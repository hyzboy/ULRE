#ifndef HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE
#define HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE

#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/math/Math.h>
#include<hgl/graph/AABB.h>
VK_NAMESPACE_BEGIN
/**
 * 单一图元数据
 */
class Primitive
{
    struct VBOData
    {
        VBO *buf;
        VkDeviceSize offset;

    public:

        CompOperatorMemcmp(const VBOData &);
    };

    Map<AnsiString,VBOData> buffer_list;

protected:

    uint32_t draw_count;

    IndexBuffer *indices_buffer=nullptr;
    VkDeviceSize indices_offset=0;

protected:

    AABB BoundingBox;

protected:

    friend class RenderableNode;

    uint ref_count=0;

    uint RefInc(){return ++ref_count;}
    uint RefDec(){return --ref_count;}

public:

    Primitive(const uint32_t dc=0):draw_count(dc){}
    virtual ~Primitive()=default;

    const   uint    GetRefCount()const{return ref_count;}

    void            SetBoundingBox(const AABB &aabb){BoundingBox=aabb;}
    const   AABB &  GetBoundingBox()const           {return BoundingBox;}

            bool Set(const AnsiString &name,VBO *vb,VkDeviceSize offset=0);

            bool Set(IndexBuffer *ib,VkDeviceSize offset=0)
            {
                if(!ib)return(false);

                indices_buffer=ib;
                indices_offset=offset;
                return(true);
            }

public:

                    void            SetDrawCount(const uint32_t dc){draw_count=dc;} ///<设置当前对象绘制需要多少个顶点
    virtual const   uint32_t        GetDrawCount()const                             ///<取得当前对象绘制需要多少个顶点
    {
        if(indices_buffer)
            return indices_buffer->GetCount();

        return draw_count;
    }

                    VBO *           GetVBO              (const AnsiString &,VkDeviceSize *);
                    VkBuffer        GetBuffer           (const AnsiString &,VkDeviceSize *);
            const   int             GetBufferCount      ()const {return buffer_list.GetCount();}

                    IndexBuffer *   GetIndexBuffer      ()      {return indices_buffer;}
            const   VkDeviceSize    GetIndexBufferOffset()const {return indices_offset;}
};//class Primitive
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PRIMITIVE_INCLUDE
