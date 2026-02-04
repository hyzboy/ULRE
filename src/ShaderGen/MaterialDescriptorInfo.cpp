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

    ubo_map.ChangeOrAdd(obj->name, (UBODescriptor *)obj);
    return((UBODescriptor *)obj);
}

const SSBODescriptor *MaterialDescriptorInfo::AddSSBO(uint32_t ssb,DescriptorSetType set_type,SSBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);
    
    ssbo_map.ChangeOrAdd(obj->name, (SSBODescriptor *)obj);
    return((SSBODescriptor *)obj);
}

const TextureDescriptor *MaterialDescriptorInfo::AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(shader_stage_flag_bits,sd);

    texture_map.ChangeOrAdd(obj->name, (TextureDescriptor *)obj);
    return((TextureDescriptor *)obj);
}

const TextureSamplerDescriptor *MaterialDescriptorInfo::AddTextureSampler(uint32_t ssb,DescriptorSetType set_type,TextureSamplerDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);

    texture_sampler_map.ChangeOrAdd(obj->name, (TextureSamplerDescriptor *)obj);
    return((TextureSamplerDescriptor *)obj);
}

UBODescriptor *MaterialDescriptorInfo::GetUBO(const AnsiString &name)
{
    UBODescriptor* const* value_ptr = ubo_map.GetValuePointer(name);
    if(value_ptr)
        return *value_ptr;

    return(nullptr);
}

SSBODescriptor *MaterialDescriptorInfo::GetSSBO(const AnsiString &name)
{
    SSBODescriptor* const* value_ptr = ssbo_map.GetValuePointer(name);
    if(value_ptr)
        return *value_ptr;

    return(nullptr);
}

TextureDescriptor *MaterialDescriptorInfo::GetTexture(const AnsiString &name)
{
    TextureDescriptor* const* value_ptr = texture_map.GetValuePointer(name);
    if(value_ptr)
        return *value_ptr;

    return(nullptr);
}

TextureSamplerDescriptor *MaterialDescriptorInfo::GetTextureSampler(const AnsiString &name)
{
    TextureSamplerDescriptor* const* value_ptr = texture_sampler_map.GetValuePointer(name);
    if(value_ptr)
        return *value_ptr;

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

        int i = 0;
        std::vector<AnsiString> keys;
        p.descriptor_map.GetKeyArray(keys);
        for(const auto &key:keys)
        {
            if(!p.descriptor_map.ContainsKey(key))continue;
            auto* sd=p.descriptor_map.GetValueRef(key);
            sd->set = set;
            sd->binding = i;
            ++i;
        }

        ++set;
    }
}
}}//namespace hgl::graph
