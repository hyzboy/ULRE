#ifndef HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE
#define HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE

#include"VK.h"
#include<hgl/type/BaseString.h>
VK_NAMESPACE_BEGIN
class VertexBuffer;
class IndexBuffer;
class Shader;

/**
 * 顶点输入状态实例<br>
 * 本对象用于传递给MaterialInstance,用于已经确定好顶点格式的情况下，依然可修改部分设定(如instance)。
 */
class VertexAttributeBinding
{
    Shader *shader;
    VkVertexInputBindingDescription *binding_list;

private:

    friend class Shader;

    VertexAttributeBinding(Shader *);

public:

    ~VertexAttributeBinding();

    bool SetInstance(const uint index,bool instance);
    bool SetInstance(const UTF8String &name,bool instance);

    void Write(VkPipelineVertexInputStateCreateInfo &vis)const;
};//class VertexInputStateInstance

/**
 * 顶点输入配置，负责具体的buffer绑定，提供给CommandBuffer使用<br>
 * 注：本引擎不支持一个Buffer中包括多种数据
 */
class VertexInput
{
    const Shader *shader;

    int buf_count=0;
    VkBuffer *buf_list=nullptr;
    VkDeviceSize *buf_offset=nullptr;
    
    IndexBuffer *indices_buffer=nullptr;
    VkDeviceSize indices_offset=0;

public:

    VertexInput(const Shader *);
    virtual ~VertexInput();

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

    const int               GetCount    ()const{return buf_count;}
    const VkBuffer *        GetBuffer   ()const{return buf_list;}
    const VkDeviceSize *    GetOffset   ()const{return buf_offset;}

          IndexBuffer *     GetIndexBuffer()     {return indices_buffer;}
    const VkDeviceSize      GetIndexOffset()const{return indices_offset;}
};//class VertexInput
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE
