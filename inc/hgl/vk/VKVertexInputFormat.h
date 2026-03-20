#pragma once

#include<vulkan/vulkan.h>
#include<hgl/vk/VertexAttrib.h>

namespace hgl::graph
{
    struct VertexInputFormat
    {
        VkFormat    format;
        uint        vec_size;
        uint        stride;

        VertexAttrib        attrib;
        int                 binding;
        VkVertexInputRate   input_rate;
    };//struct VertexInputFormat

    using VIF=VertexInputFormat;
}//namespace hgl::graph
