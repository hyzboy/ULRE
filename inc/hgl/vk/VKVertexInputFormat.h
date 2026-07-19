#pragma once

#include<vulkan/vulkan.h>
#include<hgl/vk/VertexAttrib.h>

namespace hgl::graph
{
    struct VertexInputFormat
    {
        VertexSemantic semantic;
        VkFormat    format;
        uint        vec_size;
        uint        stride;

        const char *        name;
        int                 binding;
    };//struct VertexInputFormat

    using VIF=VertexInputFormat;
}//namespace hgl::graph
