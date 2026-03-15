#pragma once

#include<hgl/CoreType.h>
#include<hgl/mtl/DescriptorKind.h>
#include<hgl/common/DescriptorSetTypeDef.h>

namespace hgl::graph::mtl
{
    struct FixedDescriptorEntry
    {
        DescriptorSetType   set_type;
        DescriptorKind      kind;
        uint32_t            stage_flags;
        const char *        name;
        const char *        struct_name;
        const char *        glsl_type;
    };
}//namespace hgl::graph::mtl
