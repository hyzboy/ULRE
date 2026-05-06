#include<hgl/shadergen/BindingContractBuilder.h>

namespace hgl::graph::mtl
{
void BindingContractBuilder::Build(MaterialDescriptorDB &descriptor_db,DescriptorBindingSlots &binding_contract)
{
    binding_contract = DescriptorBindingSlots{};

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
    {
        const UBODescriptor *d = descriptor_db.GetUBO(UBODescriptorSemantic(i));
        if(d)
            binding_contract.ubos[i] = d->stage_flag;
    }

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
    {
        const SSBODescriptor *d = descriptor_db.GetSSBO(SSBODescriptorSemantic(i));
        if(d)
            binding_contract.ssbos[i] = d->stage_flag;
    }
}
}//namespace hgl::graph::mtl
