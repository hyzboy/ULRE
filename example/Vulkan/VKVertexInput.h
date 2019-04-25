#ifndef HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE
#define HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class VertexBuffer;
class IndexBuffer;

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
    List<VkDeviceSize> buf_offset;

    List<VkVertexInputBindingDescription> binding_list;
    List<VkVertexInputAttributeDescription> attribute_list;

    IndexBuffer *indices_buffer=nullptr;
    VkDeviceSize indices_offset=0;

public:

    VertexInput()=default;
    virtual ~VertexInput()=default;

    bool Add(uint32_t location,VertexBuffer *,bool instance=false,VkDeviceSize offset=0);
    bool AddIndices(IndexBuffer *ib,VkDeviceSize offset=0)
    {
        if(!ib)return(false);

        indices_buffer=ib;
        indices_offset=offset;
    }

public:

    const uint              GetCount    ()const{return buf_list.GetCount();}
    const VkBuffer *        GetBuffer   ()const{return buf_list.GetData();}
    const VkDeviceSize *    GetOffset   ()const{return buf_offset.GetData();}

          IndexBuffer *     GetIndexBuffer()const{return indices_buffer;}
    const VkDeviceSize      GetIndexOffset()const{return indices_offset;}

    const VkPipelineVertexInputStateCreateInfo GetPipelineVertexInputStateCreateInfo()const;
};//class VertexInput
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE
