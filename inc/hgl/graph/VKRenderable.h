#ifndef HGL_GRAPH_VULKAN_RENDERABLE_INCLUDE
#define HGL_GRAPH_VULKAN_RENDERABLE_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/math/Math.h>
VK_NAMESPACE_BEGIN
/**
 * 可渲染数据对象<br>
 * 本对象包含材质实例信息和Mesh信息，可渲染数据对象中包含供最终API使用的VBO数据，可能存在多个MESH数据的集合。</p>
 * 比如有多种形状的石头，它们使用了同一种材质，这种情况下多个mesh就可以合并到一个Renderable中，渲染时不再切换VBO。
 */
class Renderable
{
    struct VABData
    {
        VAB *buf;
        VkDeviceSize offset;

    public:

        CompOperatorMemcmp(const VABData &);
    };

    Map<UTF8String,VABData> buffer_list;
    
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

    Renderable(const uint32_t dc=0):draw_count(dc){}
    virtual ~Renderable()=default;

    const uint GetRefCount()const{return ref_count;}

    void        SetBoundingBox(const AABB &aabb){BoundingBox=aabb;}
    const AABB &GetBoundingBox()const           {return BoundingBox;}

    bool Set(const UTF8String &name,VAB *vb,VkDeviceSize offset=0);

    bool Set(IndexBuffer *ib,VkDeviceSize offset=0)
    {
        if(!ib)return(false);

        indices_buffer=ib;
        indices_offset=offset;
        return(true);
    }

public:

                    void        SetDrawCount(const uint32_t dc){draw_count=dc;} ///<设置当前对象绘制需要多少个顶点
    virtual const   uint32_t    GetDrawCount()const                             ///<取得当前对象绘制需要多少个顶点
    {
        if(indices_buffer)
            return indices_buffer->GetCount();

        return draw_count;
    }

            VAB *           GetVAB(const UTF8String &,VkDeviceSize *);
            VkBuffer        GetBuffer(const UTF8String &,VkDeviceSize *);
    const   int             GetBufferCount()const{return buffer_list.GetCount();}

    IndexBuffer *           GetIndexBuffer()            {return indices_buffer;}
    const VkDeviceSize      GetIndexBufferOffset()const {return indices_offset;}
};//class Renderable
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDERABLE_INCLUDE
