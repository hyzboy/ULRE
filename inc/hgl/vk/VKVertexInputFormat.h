#pragma once
#include<hgl/vk/VKNamespace.h>
#include<hgl/vk/VertexAttrib.h>
namespace hgl::graph{
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
}//namespace hgl::graph
