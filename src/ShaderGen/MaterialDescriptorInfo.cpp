#include <string>
#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<vector>

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

const DescriptorSetType MaterialDescriptorInfo::GetSetType(const std::string &name)const
{
    for(auto &sds:desc_set_array)
        ShaderDescriptor *sd = nullptr;
        if(sds.descriptor_map.Get(name,sd))
            return(sds.set_type);

    return DescriptorSetType::Global;
}

const UBODescriptor *MaterialDescriptorInfo::AddUBO(uint32_t ssb,DescriptorSetType set_type,UBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);

    ubo_map[obj->name] = (UBODescriptor *)obj;
    return((UBODescriptor *)obj);
}

const SSBODescriptor *MaterialDescriptorInfo::AddSSBO(uint32_t ssb,DescriptorSetType set_type,SSBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);

    ssbo_map[obj->name] = (SSBODescriptor *)obj;
    return((SSBODescriptor *)obj);
}

const TextureDescriptor *MaterialDescriptorInfo::AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(shader_stage_flag_bits,sd);

    texture_map[obj->name] = (TextureDescriptor *)obj;
    return((TextureDescriptor *)obj);
}

const TextureSamplerDescriptor *MaterialDescriptorInfo::AddTextureSampler(uint32_t ssb,DescriptorSetType set_type,TextureSamplerDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);

    texture_sampler_map[obj->name] = (TextureSamplerDescriptor *)obj;
    return((TextureSamplerDescriptor *)obj);
}

UBODescriptor *MaterialDescriptorInfo::GetUBO(const std::string &name)
{
    auto it = ubo_map.find(name);
    return it != ubo_map.end() ? it->second : nullptr;
}

SSBODescriptor *MaterialDescriptorInfo::GetSSBO(const std::string &name)
{
    auto it = ssbo_map.find(name);
    return it != ssbo_map.end() ? it->second : nullptr;
}

TextureDescriptor *MaterialDescriptorInfo::GetTexture(const std::string &name)
{
    auto it = texture_map.find(name);
    return it != texture_map.end() ? it->second : nullptr;
}

TextureSamplerDescriptor *MaterialDescriptorInfo::GetTextureSampler(const std::string &name)
{
    auto it = texture_sampler_map.find(name);
    return it != texture_sampler_map.end() ? it->second : nullptr;
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

        int i = 0;
        std::vector<ShaderDescriptor *> values;
        p.descriptor_map.GetValueArray(values);
        for(ShaderDescriptor *sd : values)
        {
            if(!sd) continue;
            sd->set = set;
            sd->binding = i;
            ++i;
        }

        ++set;
    }
}
}}//namespace hgl::graph
