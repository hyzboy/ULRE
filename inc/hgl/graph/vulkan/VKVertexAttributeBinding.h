#ifndef HGL_GRAPH_VULKAN_VERTEX_ATTRIBUTE_BINDING_INCLUDE
#define HGL_GRAPH_VULKAN_VERTEX_ATTRIBUTE_BINDING_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/type/String.h>
VK_NAMESPACE_BEGIN
/**
* 顶点输入状态实例<br>
* 本对象用于传递给Material,用于已经确定好顶点格式的情况下，依然可修改部分设定(如instance)。
*/
class VertexAttributeBinding
{
    uint32_t attr_count;
    VkVertexInputBindingDescription *binding_list;
    VkVertexInputAttributeDescription *attribute_list;

private:

    friend class VertexShaderModule;

    VertexAttributeBinding(const uint32_t,const VkVertexInputBindingDescription *,const VkVertexInputAttributeDescription *);

public:

    ~VertexAttributeBinding();

    bool SetInstance(const uint binding,bool              instance);
    bool SetStride  (const uint binding,const uint32_t &  stride);
    bool SetFormat  (const uint binding,const VkFormat &  format);
    bool SetOffset  (const uint binding,const uint32_t    offset);

    void Write(VkPipelineVertexInputStateCreateInfo &vis)const;
};//class VertexAttributeBinding
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_VERTEX_ATTRIBUTE_BINDING_INCLUDE
