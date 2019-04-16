#ifndef HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE
#define HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE

#include"VK.h"
#include"VKBuffer.h"
VK_NAMESPACE_BEGIN
/**
 * 顶点输入配置，类似于OpenGL的VAB<br>
 * 注：本引擎不支持一个BUFFER中包括多种数据
 */
class VertexInput
{
    struct VertexInputBuffer
    {
        //按API，可以一个binding绑多个attrib，但我们仅支持1v1

        VkVertexInputBindingDescription binding;
        VkVertexInputAttributeDescription attrib;
        VertexBuffer *buffer;

    public:

        VertexInputBuffer(VkVertexInputBindingDescription bind,VkVertexInputAttributeDescription attr,VertexBuffer *buf)
        {
            binding=bind;
            attrib=attr;
            buffer=buf;
        }
    };

    ObjectList<VertexInputBuffer> vib_list;
    List<VkBuffer> buf_list;

public:

    bool Add(VertexBuffer *);

public:

    const List<VkBuffer> &GetBufferList()const{return buf_list;}
};//class VertexInput
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE
