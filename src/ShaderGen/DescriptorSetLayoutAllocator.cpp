#include<hgl/shadergen/DescriptorSetLayoutAllocator.h>
namespace hgl{namespace graph::shadergen{
    using namespace hgl::graph::mtl;
DescriptorSetLayoutAllocator::DescriptorSetLayoutAllocator()
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

const DescriptorSetType DescriptorSetLayoutAllocator::GetSetType(const std::string &name)const
{
    for(auto &sds:desc_set_array)
        if(sds.descriptor_map.ContainsKey(name.c_str()))
            return(sds.set_type);

    return DescriptorSetType::Unknow;
}

const UBODescriptor *DescriptorSetLayoutAllocator::AddUBO(uint32_t ssb,DescriptorSetType set_type,UBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);

    ubo_map[obj->name] = (UBODescriptor *)obj;
    return((UBODescriptor *)obj);
}

const SSBODescriptor *DescriptorSetLayoutAllocator::AddSSBO(uint32_t ssb,DescriptorSetType set_type,SSBODescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);

    ssbo_map[obj->name] = (SSBODescriptor *)obj;
    return((SSBODescriptor *)obj);
}

const TextureDescriptor *DescriptorSetLayoutAllocator::AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(shader_stage_flag_bits,sd);

    texture_map[obj->name] = (TextureDescriptor *)obj;
    return((TextureDescriptor *)obj);
}

const TextureSamplerDescriptor *DescriptorSetLayoutAllocator::AddTextureSampler(uint32_t ssb,DescriptorSetType set_type,TextureSamplerDescriptor *sd)
{
    RANGE_CHECK_RETURN_NULLPTR(set_type);
    if(!sd)return(nullptr);

    ShaderDescriptorSet *sds=desc_set_array+(size_t)set_type;

    ShaderDescriptor *obj=sds->AddDescriptor(ssb,sd);

    texture_sampler_map[obj->name] = (TextureSamplerDescriptor *)obj;
    return((TextureSamplerDescriptor *)obj);
}

UBODescriptor *DescriptorSetLayoutAllocator::GetUBO(const std::string &name)
{
    const auto iter=ubo_map.find(name);
    if(iter!=ubo_map.end())
        return iter->second;

    return(nullptr);
}

SSBODescriptor *DescriptorSetLayoutAllocator::GetSSBO(const std::string &name)
{
    const auto iter=ssbo_map.find(name);
    if(iter!=ssbo_map.end())
        return iter->second;

    return(nullptr);
}

TextureDescriptor *DescriptorSetLayoutAllocator::GetTexture(const std::string &name)
{
    const auto iter=texture_map.find(name);
    if(iter!=texture_map.end())
        return iter->second;

    return(nullptr);
}

TextureSamplerDescriptor *DescriptorSetLayoutAllocator::GetTextureSampler(const std::string &name)
{
    const auto iter=texture_sampler_map.find(name);
    if(iter!=texture_sampler_map.end())
        return iter->second;

    return(nullptr);
}
}}//namespace hgl::graph::shadergen

