#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl{namespace graph{
ShaderDescriptorInfo::ShaderDescriptorInfo(VkShaderStageFlagBits flag_bit)
{
    stage_flag=flag_bit;

    Init(stage_io);
    
    hgl_zero(push_constant);
}

namespace
{
    bool Find(const ShaderAttributeArray &sad,const char *name)
    {
        if(sad.count<=0)
            return(false);

        for(uint i=0;i<sad.count;i++)
            if(hgl::strcmp(sad.items[i].name,name)==0)
                return(true);

        return(false);
    }
}//namespace

bool ShaderDescriptorInfo::AddInput(ShaderAttribute *ss)
{
    if(!ss)return(false);

    if(Find(stage_io.input,ss->name))return(false);

    ss->location=stage_io.input.count;

    Append(&stage_io.input,ss);
    return(true);
}


bool ShaderDescriptorInfo::hasInput(const char *name)const
{
    if(!name||!*name)return(false);

    return Find(stage_io.input,name);

}

bool ShaderDescriptorInfo::AddOutput(ShaderAttribute *ss)
{
    if(!ss)return(false);

    if(Find(stage_io.output,ss->name))return(false);
    
    ss->location=stage_io.output.count;

    Append(&stage_io.output,ss);
    return(true);
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

bool ShaderDescriptorInfo::AddSubpassInput(const UTF8String name,uint8_t index)
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

void ShaderDescriptorInfo::SetPushConstant(const UTF8String name,uint8_t offset,uint8_t size)
{
    push_constant.name  =name;
    push_constant.offset=offset;
    push_constant.size  =size;
}
}}//namespace hgl::graph