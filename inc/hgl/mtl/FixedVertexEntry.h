#pragma once

#include<hgl/vk/VKVertexInputAttribute.h>
#include<hgl/vk/VertexAttrib.h>

namespace hgl::graph::mtl{

struct FixedVertexEntry
{
    VAType              type;
    VertexInputGroup    group;
    VertexInputRate     input_rate;
    VertexAttrib        attrib;
};

}//namespace hgl::graph::mtl
