#include<hgl/shadergen/DescriptorLayoutBuilder.h>
#include<hgl/shadergen/BindingContractBuilder.h>

namespace hgl::graph::mtl
{
void DescriptorLayoutBuilder::Finalize(MaterialDescriptorDB &descriptor_db,DescriptorBindingSlots &binding_contract)
{
    descriptor_db.Resort();
    BindingContractBuilder::Build(descriptor_db,binding_contract);
}
}//namespace hgl::graph::mtl
