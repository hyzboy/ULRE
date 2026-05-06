#pragma once

#include<hgl/shadergen/MaterialDescriptorDB.h>
#include<hgl/mtl/DescriptorSemanticRegistry.h>

namespace hgl::graph::mtl
{
class BindingContractBuilder
{
public:
    static void Build(MaterialDescriptorDB &descriptor_db,DescriptorBindingSlots &binding_contract);
};
}//namespace hgl::graph::mtl
