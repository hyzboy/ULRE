#pragma once

#include<hgl/CoreType.h>
#include<hgl/vk/VKDescriptorSetType.h>

namespace hgl::graph::mtl{

enum class DescriptorKind : uint8
{
    UBO,
    SSBO,
    Texture,
    TextureSampler,
};

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
