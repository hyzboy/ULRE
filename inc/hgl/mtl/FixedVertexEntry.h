#pragma once

#include<vulkan/vulkan.h>
#include<hgl/common/VertexAttribDef.h>

namespace hgl::graph::mtl{

struct FixedVertexEntry
{
    VkFormat        format;
    VertexSemantic  semantic;

    bool operator==(const FixedVertexEntry &rhs) const noexcept
    {
        return format == rhs.format && semantic == rhs.semantic;
    }
};

}//namespace hgl::graph::mtl
