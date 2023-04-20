#include<hgl/shadergen/MaterialDescriptorInfo.h>

namespace hgl{namespace graph{
MaterialDescriptorInfo::MaterialDescriptorInfo()
{
    int set_type=(int)DescriptorSetType::BEGIN_RANGE;

    for(auto &p:desc_set_array)
    {
        p.set_type=(DescriptorSetType)set_type;

        ++set_type;

        p.set=-1;
        p.count=0;
    }

    descriptor_count=0;
}

const DescriptorSetType MaterialDescriptorInfo::GetSetType(const AnsiString &name)const
{
    for(auto &sds:desc_set_array)
        if(sds.descriptor_map.KeyExist(name))
            return(sds.set_type);

    return DescriptorSetType::Global;
}

const UBODescriptor *MaterialDescriptorInfo::AddUBO(VkShaderStageFlagBits ssb,DescriptorSetType set_type,UBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);
    
    ubo_map.Add(obj->name,(UBODescriptor *)obj);
    return((UBODescriptor *)obj);
}

const SamplerDescriptor *MaterialDescriptorInfo::AddSampler(VkShaderStageFlagBits ssb,DescriptorSetType set_type,SamplerDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);
    
    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);

    sampler_map.Add(obj->name,(SamplerDescriptor *)obj);
    return((SamplerDescriptor *)obj);
}

UBODescriptor *MaterialDescriptorInfo::GetUBO(const AnsiString &name)
{
    UBODescriptor *sd;

    if(ubo_map.Get(name,sd))
        return(sd);

    return(nullptr);
}

SamplerDescriptor *MaterialDescriptorInfo::GetSampler(const AnsiString &name)
{
    SamplerDescriptor *sd;

    if(sampler_map.Get(name,sd))
        return(sd);

    return(nullptr);
}

void MaterialDescriptorInfo::Resort()
{
    descriptor_count=0;

    //重新生成set/binding
    {
        int set=0;

        for(auto &p:desc_set_array)
        {
            if(p.count>0)
            {
                descriptor_count+=p.count;

                p.set=set;

                auto *sdp=p.descriptor_map.GetDataList();
                for(int i=0;i<p.descriptor_map.GetCount();i++)
                {
                    (*sdp)->value->set=set;
                    (*sdp)->value->binding=i;

                    ++sdp;
                }

                ++set;
            }
        }
    }
}
}}//namespace hgl::graph