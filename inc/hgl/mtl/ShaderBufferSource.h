#pragma once

#include<hgl/mtl/DescriptorBindingContract.h>

namespace hgl::graph
{
    struct ShaderBufferDesc
    {
        const DescriptorSetType set_type;
        const mtl::DescriptorKind kind;
        const mtl::UBODescriptorSemantic ubo_semantic;
        const mtl::SSBODescriptorSemantic ssbo_semantic;

        const char *name;
    };
}//namespace hgl::graph
