#pragma once

#include<hgl/mtl/FixedVertexEntry.h>
#include<hgl/mtl/DescriptorBindingContract.h>
#include<hgl/vk/VKPrimitiveType.h>

namespace hgl::graph::mtl{

struct FixedMaterialDef
{
    const char *                name;

    PrimitiveType               primitive_type;

    const FixedVertexEntry *    vertex_entries;
    uint32_t                    vertex_entry_count;

    const FixedDescriptorEntry *descriptor_entries;
    uint32_t                    descriptor_entry_count;

    const char *                mi_glsl_codes;
    uint32_t                    mi_struct_bytes;
};

}//namespace hgl::graph::mtl
