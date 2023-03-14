#include<hgl/shadergen/MaterialDescriptorManager.h>

SHADERGEN_NAMESPACE_BEGIN
MaterialDescriptorManager::MaterialDescriptorManager()
{
    int set_type=(int)DescriptorSetType::BEGIN_RANGE;

    for(auto &p:desc_set_array)
    {
        p.set_type=(DescriptorSetType)set_type;

        ++set_type;

        p.set=-1;
        p.count=0;
    }
}

const DescriptorSetType MaterialDescriptorManager::GetSetType(const AnsiString &name)const
{
    for(auto &sds:desc_set_array)
        if(sds.descriptor_map.KeyExist(name))
            return(sds.set_type);

    return DescriptorSetType::Global;
}

/**
* 添加一个描述符，如果它本身存在，则返回false
*/
ShaderDescriptor *MaterialDescriptorManager::ShaderDescriptorSet::AddDescriptor(VkShaderStageFlagBits ssb,ShaderDescriptor *new_sd)
{
    ShaderDescriptor *sd;
    
    if(descriptor_map.Get(new_sd->name,sd))
    {
        delete new_sd;
        sd->stage_flag|=ssb;
        return(sd);
    }
    else
    {
        new_sd->set_type=set_type;
        new_sd->stage_flag=ssb;

        descriptor_map.Add(new_sd->name,new_sd);

        count++;

        return(new_sd);
    }
}

const UBODescriptor *MaterialDescriptorManager::AddUBO(VkShaderStageFlagBits ssb,DescriptorSetType set_type,UBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);
    
    ubo_map.Add(obj->name,(UBODescriptor *)obj);
    return((UBODescriptor *)obj);
}

const SamplerDescriptor *MaterialDescriptorManager::AddSampler(VkShaderStageFlagBits ssb,DescriptorSetType set_type,SamplerDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);
    
    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);

    sampler_map.Add(obj->name,(SamplerDescriptor *)obj);
    return((SamplerDescriptor *)obj);
}

UBODescriptor *MaterialDescriptorManager::GetUBO(const AnsiString &name)
{
    UBODescriptor *sd;

    if(ubo_map.Get(name,sd))
        return(sd);

    return(nullptr);
}

SamplerDescriptor *MaterialDescriptorManager::GetSampler(const AnsiString &name)
{
    SamplerDescriptor *sd;

    if(sampler_map.Get(name,sd))
        return(sd);

    return(nullptr);
}

void MaterialDescriptorManager::Resort()
{
    //重新生成set/binding
    {
        int set=0;

        for(auto &p:desc_set_array)
        {
            if(p.count>0)
            {
                p.set=set;

                auto *sdp=p.descriptor_map.GetDataList();
                for(int i=0;i<p.descriptor_map.GetCount();i++)
                {
                    (*sdp)->right->set=set;
                    (*sdp)->right->binding=i;

                    ++sdp;
                }

                ++set;
            }
        }
    }
}
SHADERGEN_NAMESPACE_END