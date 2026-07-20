#pragma once

#include<vulkan/vulkan.h>
#include<hgl/common/VertexAttribDef.h>

namespace hgl::graph::mtl{

struct FixedVertexEntry
{
    VkFormat        format;
    VertexSemantic  semantic;
};

}//namespace hgl::graph::mtl
