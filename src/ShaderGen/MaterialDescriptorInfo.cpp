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
        if(sds.descriptor_map.ContainsKey(name))
            return(sds.set_type);

    return DescriptorSetType::Global;
}

const UBODescriptor *MaterialDescriptorInfo::AddUBO(uint32_t ssb,DescriptorSetType set_type,UBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);
    
    ubo_map.Add(obj->name,(UBODescriptor *)obj);
    return((UBODescriptor *)obj);
}

const TextureDescriptor *MaterialDescriptorInfo::AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(shader_stage_flag_bits,sd);

    texture_map.Add(obj->name,(TextureDescriptor *)obj);
    return((TextureDescriptor *)obj);
}

const TextureSamplerDescriptor *MaterialDescriptorInfo::AddTextureSampler(uint32_t ssb,DescriptorSetType set_type,TextureSamplerDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);
    
    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);

    texture_sampler_map.Add(obj->name,(TextureSamplerDescriptor *)obj);
    return((TextureSamplerDescriptor *)obj);
}

UBODescriptor *MaterialDescriptorInfo::GetUBO(const AnsiString &name)
{
    UBODescriptor *sd;

    if(ubo_map.Get(name,sd))
        return(sd);

    return(nullptr);
}

TextureDescriptor *MaterialDescriptorInfo::GetTexture(const AnsiString &name)
{
    TextureDescriptor *sd;

    if(texture_map.Get(name,sd))
        return(sd);

    return(nullptr);
}

TextureSamplerDescriptor *MaterialDescriptorInfo::GetTextureSampler(const AnsiString &name)
{
    TextureSamplerDescriptor *sd;

    if(texture_sampler_map.Get(name,sd))
        return(sd);

    return(nullptr);
}

void MaterialDescriptorInfo::Resort()
{
    descriptor_count=0;

    //重新生成set/binding
    int set=0;

    for(auto &p:desc_set_array)
    {
        if(p.count<=0)
            continue;
        
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
}}//namespace hgl::graph
