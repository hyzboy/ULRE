#pragma once

#include<hgl/vk/VKVertexInputAttribute.h>
#include<hgl/vk/VertexAttrib.h>

namespace hgl::graph::mtl{

struct FixedVertexEntry
{
    VAType              type;
    VertexAttrib        attrib;
};

}//namespace hgl::graph::mtl
