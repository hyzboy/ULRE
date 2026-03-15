#pragma once

#include<hgl/common/DescriptorSetTypeDef.h>

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
    };
}//namespace hgl::graph
