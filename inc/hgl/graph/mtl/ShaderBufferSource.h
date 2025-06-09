#pragma once

#include<hgl/graph/VKDescriptorSetType.h>

namespace hgl::graph
{
    struct ShaderBufferDesc
    {
        const DescriptorSetType set_type;

        const char *name;
    };

    struct ShaderBufferSource:public ShaderBufferDesc
    {
        const char *struct_name;
        const char *codes;
    };
}//namespace hgl::graph
