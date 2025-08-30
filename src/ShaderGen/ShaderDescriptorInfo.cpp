#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl{namespace graph{
ShaderDescriptorInfo::ShaderDescriptorInfo(ShaderStage flag_bit)
{
    stage_flag=flag_bit;
   
    hgl_zero(push_constant);
}

void ShaderDescriptorInfo::AddStruct(const AnsiString &name)
{
    struct_list.AddUnique(name);
}

bool ShaderDescriptorInfo::AddUBO(DescriptorSetType type,const UBODescriptor *ubo)
{
    if(!ubo)
        return(false);

    struct_list.AddUnique(ubo->type);

    ubo_list.Add(ubo);
    return true;
}

bool ShaderDescriptorInfo::AddSampler(DescriptorSetType type,const SamplerDescriptor *sampler)
{
    if(!sampler)
        return(false);

    sampler_list.Add(sampler);
    return true;
}

bool ShaderDescriptorInfo::AddConstValue(ConstValueDescriptor *sd)
{
    if(!sd)return(false);

    for(auto *p:const_value_list)
        if(p->name.Comp(sd->name)==0)
            return(false);

    sd->constant_id=const_value_list.Add(sd);
    return(true);
}

bool VertexShaderDescriptorInfo::AddSubpassInput(const AnsiString &name,uint8_t index)
{
    for(auto *si:subpass_input)
    {
        if(si->input_attachment_index==index)return(false);
        if(si->name.Comp(name))return(false);
    }

    SubpassInputDescriptor *ssi=new SubpassInputDescriptor;

    ssi->name=name;
    ssi->input_attachment_index=index;

    subpass_input.Add(ssi);
    return(true);
}

void ShaderDescriptorInfo::SetPushConstant(const AnsiString &name,uint8_t offset,uint8_t size)
{
    push_constant.name  =name;
    push_constant.offset=offset;
    push_constant.size  =size;
}
}}//namespace hgl::graph