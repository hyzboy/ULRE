#pragma once
#include<hgl/vk/VKNamespace.h>
#include<hgl/vk/VertexAttrib.h>
VK_NAMESPACE_BEGIN
struct VertexInputFormat
{
    VkFormat    format;
    uint        vec_size;
    uint        stride;

    const char *        name;
    int                 binding;
    VkVertexInputRate   input_rate;
    VertexInputGroup    group;
};//struct VertexInputFormat

using VIF=VertexInputFormat;
VK_NAMESPACE_END
