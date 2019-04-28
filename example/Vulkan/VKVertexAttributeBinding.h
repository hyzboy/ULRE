#ifndef HGL_GRAPH_VULKAN_VERTEX_ATTRIBUTE_BINDING_INCLUDE
#define HGL_GRAPH_VULKAN_VERTEX_ATTRIBUTE_BINDING_INCLUDE

#include"VK.h"
#include<hgl/type/BaseString.h>
VK_NAMESPACE_BEGIN
class VertexBuffer;
class IndexBuffer;
class VertexShaderModule;

/**
* 顶点输入状态实例<br>
* 本对象用于传递给MaterialInstance,用于已经确定好顶点格式的情况下，依然可修改部分设定(如instance)。
*/
class VertexAttributeBinding
{
    VertexShaderModule *vsm;
    uint32_t attr_count;
    VkVertexInputBindingDescription *binding_list;
    VkVertexInputAttributeDescription *attribute_list;

private:

    friend class VertexShaderModule;

    VertexAttributeBinding(VertexShaderModule *);

public:

    ~VertexAttributeBinding();

    const uint GetBinding(const UTF8String &name);                                                  ///<取得一个变量的绑定点

    bool SetInstance(const uint binding,bool              instance);
    bool SetStride  (const uint binding,const uint32_t &  stride);
    bool SetFormat  (const uint binding,const VkFormat &  format);
    bool SetOffset  (const uint binding,const uint32_t    offset);

    bool SetInstance(const UTF8String &name,bool            instance){return SetInstance(GetBinding(name),instance);}
    bool SetStride  (const UTF8String &name,const uint32_t &stride  ){return SetStride  (GetBinding(name),stride);}
    bool SetFormat  (const UTF8String &name,const VkFormat &format  ){return SetFormat  (GetBinding(name),format);}
    bool SetOffset  (const UTF8String &name,const uint32_t  offset  ){return SetOffset  (GetBinding(name),offset);}

    void Write(VkPipelineVertexInputStateCreateInfo &vis)const;
};//class VertexAttributeBinding
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_VERTEX_ATTRIBUTE_BINDING_INCLUDE
