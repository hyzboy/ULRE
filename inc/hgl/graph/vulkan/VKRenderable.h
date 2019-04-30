#ifndef HGL_GRAPH_VULKAN_RENDERABLE_INCLUDE
#define HGL_GRAPH_VULKAN_RENDERABLE_INCLUDE

#include"VK.h"
#include<hgl/type/BaseString.h>
VK_NAMESPACE_BEGIN
class VertexShaderModule;
class VertexBuffer;
class IndexBuffer;

/**
* 可渲染对象<br>
* 本对象包含材质实例信息和Mesh信息
*/
class Renderable
{
    const VertexShaderModule *vertex_sm;

    int buf_count;
    VkBuffer *buf_list=nullptr;
    VkDeviceSize *buf_offset=nullptr;

    IndexBuffer *indices_buffer=nullptr;
    VkDeviceSize indices_offset=0;

public:

    Renderable(const VertexShaderModule *);
    virtual ~Renderable();

    bool Set(const int binding,     VertexBuffer *vb,VkDeviceSize offset=0);
    bool Set(const UTF8String &name,VertexBuffer *vb,VkDeviceSize offset=0);

    bool Set(IndexBuffer *ib,VkDeviceSize offset=0)
    {
        if(!ib)return(false);

        indices_buffer=ib;
        indices_offset=offset;
        return(true);
    }

public:

    const int               GetBufferCount  ()const{return buf_count;}
    const VkBuffer *        GetBuffer       ()const{return buf_list;}
    const VkDeviceSize *    GetOffset       ()const{return buf_offset;}

    IndexBuffer *           GetIndexBuffer()     {return indices_buffer;}
    const VkDeviceSize      GetIndexOffset()const{return indices_offset;}
};//class Renderable
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDERABLE_INCLUDE
