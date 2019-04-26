#ifndef HGL_GRAPH_VULKAN_VERTEX_ATTRIBUTE_BINDING_INCLUDE
#define HGL_GRAPH_VULKAN_VERTEX_ATTRIBUTE_BINDING_INCLUDE

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
};//class VertexAttributeBinding
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_VERTEX_ATTRIBUTE_BINDING_INCLUDE
