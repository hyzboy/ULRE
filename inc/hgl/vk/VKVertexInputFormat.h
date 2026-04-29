#pragma once

#include<vulkan/vulkan.h>
#include<hgl/vk/VertexAttrib.h>

namespace hgl::graph
{
    enum class PositionType
    {
        None,
        Vec2,
        Vec3,
        PCG
    };

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
