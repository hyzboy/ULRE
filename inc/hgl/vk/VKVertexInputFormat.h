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
        const char *        name;           ///<GLSL variable name (from shader reflection)
        int                 binding;
        VkVertexInputRate   input_rate;
        VertexInputGroup    group;
    };//struct VertexInputFormat

    using VIF=VertexInputFormat;
}//namespace hgl::graph
