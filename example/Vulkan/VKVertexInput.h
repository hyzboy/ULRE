#ifndef HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE
#define HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class VertexBuffer;
class IndexBuffer;

/**
 * 顶点输入状态<br>
 * 顶点输入状态用于记录数据是如果传递给Pipeline的，并不包含具体数据
 */
class VertexInputState
{
    List<VkVertexInputBindingDescription> binding_list;
    List<VkVertexInputAttributeDescription> attribute_list;

public:

    VertexInputState()=default;
    ~VertexInputState()=default;

    void Add(const uint32_t shader_location,const VkFormat format,uint32_t offset=0,bool instance=false);

public:

    const uint32_t GetCount()const{return binding_list.GetCount();}

    VkVertexInputBindingDescription *   GetDesc(const int index){return (index<0||index>=binding_list.GetCount()?nullptr:binding_list.GetData()+index);}
    VkVertexInputAttributeDescription * GetAttr(const int index){return (index<0||index>=attribute_list.GetCount()?nullptr:attribute_list.GetData()+index);}

    void Write(VkPipelineVertexInputStateCreateInfo &vis)const;
};//class VertexInputStateCreater

/**
 * 顶点输入配置，类似于OpenGL的VAB<br>
 * 注：本引擎不支持一个BUFFER中包括多种数据
 */
class VertexInput
{
    VertexInputState *vis;

    uint32_t buf_count=0;
    VkBuffer *buf_list=nullptr;
    VkDeviceSize *buf_offset=nullptr;
    
    IndexBuffer *indices_buffer=nullptr;
    VkDeviceSize indices_offset=0;

public:

    VertexInput(VertexInputState *);
    virtual ~VertexInput();

    bool Set(uint32_t index,VertexBuffer *,VkDeviceSize offset=0);
    bool Set(IndexBuffer *ib,VkDeviceSize offset=0)
    {
        if(!ib)return(false);

        indices_buffer=ib;
        indices_offset=offset;
        return(true);
    }

public:

    const uint32_t          GetCount    ()const{return buf_count;}
    const VkBuffer *        GetBuffer   ()const{return buf_list;}
    const VkDeviceSize *    GetOffset   ()const{return buf_offset;}

          IndexBuffer *     GetIndexBuffer()     {return indices_buffer;}
    const VkDeviceSize      GetIndexOffset()const{return indices_offset;}
};//class VertexInput
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE
