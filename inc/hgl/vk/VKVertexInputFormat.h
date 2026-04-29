#pragma once

#include<vulkan/vulkan.h>
#include<hgl/vk/VertexAttrib.h>
#include<hgl/common/PositionType.h>

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

        PositionType        position_type; // New field for position type
    };//struct VertexInputFormat

    using VIF=VertexInputFormat;
}//namespace hgl::graph
