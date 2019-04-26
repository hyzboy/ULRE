#ifndef HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE
#define HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE

#include"VK.h"
#include<hgl/type/BaseString.h>
#include<hgl/type/Map.h>
#include<hgl/type/Set.h>
VK_NAMESPACE_BEGIN
class VertexBuffer;
class IndexBuffer;
class VertexInputStateInstance;

/**
 * 顶点输入状态<br>
 * 顶点输入状态用于记录数据是如果传递给Pipeline的，并不包含具体数据<br>
 * 本类对象用于存放在Material中，只记录格式，并不能直接供pipeline使用
 */
class VertexInputState
{
    List<VkVertexInputBindingDescription> binding_list;
    List<VkVertexInputAttributeDescription> attribute_list;

    Map<UTF8String,VkVertexInputAttributeDescription *> stage_input_locations;

    Set<VertexInputStateInstance *> instance_set;

private:

    friend class Shader;

    VertexInputState()=default;
    ~VertexInputState();

    int Add(const UTF8String &name,const uint32_t shader_location,const VkFormat format,uint32_t offset=0,bool instance=false);

public:

    void Clear()
    {
        binding_list.Clear();
        attribute_list.Clear();
    }

    const uint32_t                              GetCount    ()const{return binding_list.GetCount();}

    const int                                   GetLocation (const UTF8String &)const;
    const int                                   GetBinding  (const UTF8String &)const;

    const VkVertexInputBindingDescription *     GetDescList ()const{return binding_list.GetData();}
    const VkVertexInputAttributeDescription *   GetAttrList ()const{return attribute_list.GetData();}

    const VkVertexInputBindingDescription *     GetDesc     (const int index)const{return (index<0||index>=binding_list.GetCount()?nullptr:binding_list.GetData()+index);}
    const VkVertexInputAttributeDescription *   GetAttr     (const int index)const{return (index<0||index>=attribute_list.GetCount()?nullptr:attribute_list.GetData()+index);}

public:

    VertexInputStateInstance *  CreateInstance();
    bool                        Release(VertexInputStateInstance *);
    const uint32_t              GetInstanceCount()const{return instance_set.GetCount();}
};//class VertexInputState

/**
 * 顶点输入状态实例<br>
 * 本对象用于传递给MaterialInstance,用于已经确定好顶点格式的情况下，依然可修改部分设定(如instance)。
 */
class VertexInputStateInstance
{
    VertexInputState *vis;
    VkVertexInputBindingDescription *binding_list;

private:

    friend class VertexInputState;

    VertexInputStateInstance(VertexInputState *);

public:

    ~VertexInputStateInstance();

    bool SetInstance(const uint index,bool instance);
    bool SetInstance(const UTF8String &name,bool instance){return SetInstance(vis->GetBinding(name),instance);}

    void Write(VkPipelineVertexInputStateCreateInfo &vis)const;
};//class VertexInputStateInstance

/**
 * 顶点输入配置，负责具体的buffer绑定，提供给CommandBuffer使用<br>
 * 注：本引擎不支持一个Buffer中包括多种数据
 */
class VertexInput
{
    const VertexInputState *vis;

    int buf_count=0;
    VkBuffer *buf_list=nullptr;
    VkDeviceSize *buf_offset=nullptr;
    
    IndexBuffer *indices_buffer=nullptr;
    VkDeviceSize indices_offset=0;

public:

    VertexInput(const VertexInputState *);
    virtual ~VertexInput();

    bool Set(const int binding,     VertexBuffer *vb,VkDeviceSize offset=0);
    bool Set(const UTF8String &name,VertexBuffer *vb,VkDeviceSize offset=0){return Set(vis->GetBinding(name),vb,offset);}

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
