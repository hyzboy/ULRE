#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VertexAttrib.h>

namespace hgl::graph::mtl{

struct FixedVertexEntry
{
    VAType              type;
    VertexInputGroup    group;
    VkVertexInputRate   input_rate;
    const char *        name;
};

}//namespace hgl::graph::mtl
