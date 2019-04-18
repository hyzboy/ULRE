﻿#ifndef HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE
#define HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class VertexBuffer;

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

    List<VkVertexInputBindingDescription> binding_list;
    List<VkVertexInputAttributeDescription> attribute_list;

public:

    VertexInput()=default;
    virtual ~VertexInput()=default;

    bool Add(VertexBuffer *,bool instance=false);

public:

    const uint GetCount()const{return buf_list.GetCount();}
    const List<VkBuffer> &GetBufferList()const{return buf_list;}

    const VkPipelineVertexInputStateCreateInfo GetPipelineVertexInputStateCreateInfo()const;
};//class VertexInput
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_VERTEX_INPUT_INCLUDE
