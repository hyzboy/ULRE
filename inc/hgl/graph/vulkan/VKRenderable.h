#ifndef HGL_GRAPH_VULKAN_RENDERABLE_INCLUDE
#define HGL_GRAPH_VULKAN_RENDERABLE_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKBuffer.h>
#include<hgl/type/BaseString.h>
VK_NAMESPACE_BEGIN
/**
 * 可渲染数据对象<br>
 * 本对象包含材质实例信息和Mesh信息，可渲染数据对象中包含供最终API使用的VBO数据，可能存在多个MESH数据的集合。</p>
 * 比如有多种形状的石头，它们使用了同一种材质，这种情况下多个mesh就可以合并到一个Renderable中，渲染时不再切换VBO。
 */
class Renderable
{
    const VertexShaderModule *vertex_sm;

    int buf_count;
    VkBuffer *buf_list=nullptr;
    VkDeviceSize *buf_offset=nullptr;

    uint32_t draw_count;

    IndexBuffer *indices_buffer=nullptr;
    VkDeviceSize indices_offset=0;

protected:

    friend class RenderableInstance;

    uint ref_count=0;

    uint RefInc(){return ++ref_count;}
    uint RefDec(){return --ref_count;}

public:

    Renderable(const VertexShaderModule *,const uint32_t dc);
    virtual ~Renderable();

    const uint GetRefCount()const{return ref_count;}

    bool Set(const int stage_input_binding, VertexBuffer *vb,VkDeviceSize offset=0);
    bool Set(const UTF8String &name,        VertexBuffer *vb,VkDeviceSize offset=0);

    bool Set(IndexBuffer *ib,VkDeviceSize offset=0)
    {
        if(!ib)return(false);

        indices_buffer=ib;
        indices_offset=offset;
        return(true);
    }

public:

    const uint32_t          GetDrawCount    ()const                             ///<取得当前对象绘制需要多少个顶点
    {
        if(indices_buffer)
            return indices_buffer->GetCount();

        return draw_count;
    }

    const int               GetBufferCount  ()const{return buf_count;}
    const VkBuffer *        GetBuffer       ()const{return buf_list;}
    const VkDeviceSize *    GetOffset       ()const{return buf_offset;}

    IndexBuffer *           GetIndexBuffer()     {return indices_buffer;}
    const VkDeviceSize      GetIndexOffset()const{return indices_offset;}
};//class Renderable
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDERABLE_INCLUDE
