#pragma once

#include<hgl/shadergen/MaterialDescriptorDB.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>

namespace hgl::graph::mtl
{
class DescriptorLayoutBuilder
{
public:
    static void Finalize(MaterialDescriptorDB &descriptor_db,DescriptorBindingSlots &binding_contract);
};
}//namespace hgl::graph::mtl
