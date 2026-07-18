#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<vector>
#include<algorithm>
#include<unordered_map>

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
        if(sds.descriptor_map.ContainsKey(name.c_str()))
            return(sds.set_type);

    return DescriptorSetType::Unknow;
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
    const auto iter=ubo_map.find(name);
    if(iter!=ubo_map.end())
        return iter->second;

    return(nullptr);
}

SSBODescriptor *MaterialDescriptorInfo::GetSSBO(const std::string &name)
{
    const auto iter=ssbo_map.find(name);
    if(iter!=ssbo_map.end())
        return iter->second;

    return(nullptr);
}

TextureDescriptor *MaterialDescriptorInfo::GetTexture(const std::string &name)
{
    const auto iter=texture_map.find(name);
    if(iter!=texture_map.end())
        return iter->second;

    return(nullptr);
}

TextureSamplerDescriptor *MaterialDescriptorInfo::GetTextureSampler(const std::string &name)
{
    const auto iter=texture_sampler_map.find(name);
    if(iter!=texture_sampler_map.end())
        return iter->second;

    return(nullptr);
}

void MaterialDescriptorInfo::Resort()
{
    descriptor_count = 0;

    for(auto &p:desc_set_array)
    {
        p.set = int(p.set_type);
        if(p.count <= 0)
            continue;

        descriptor_count += p.count;

        std::vector<std::string> keys;
        keys.reserve(static_cast<size_t>(p.count));

        for(const auto &kv:p.descriptor_map)
            keys.emplace_back(kv.first.c_str()?kv.first.c_str():"");

        std::sort(keys.begin(),keys.end());

        std::unordered_map<std::string,int> fixed_binding_map;
        int next_binding = 0;

        switch(p.set_type)
        {
        case DescriptorSetType::Scene:
            fixed_binding_map["camera"] = 0;
            fixed_binding_map["sky"] = 1;
            fixed_binding_map["viewport"] = 2;
            next_binding = 3;
            break;
        case DescriptorSetType::Transform:
            fixed_binding_map["l2w"] = 0;
            fixed_binding_map["l2w_index_rows"] = 1;
            next_binding = 2;
            break;
        case DescriptorSetType::VertexData:
            fixed_binding_map["VertexDataBuffer"] = 18;
            fixed_binding_map["IndexDataBuffer"] = 19;
            fixed_binding_map["vtx_data"] = 18;
            fixed_binding_map["idx_data"] = 19;
            next_binding = 20;
            break;
        default:
            break;
        }

        for(const auto &key:keys)
        {
            if(!p.descriptor_map.ContainsKey(key.c_str()))
                continue;

            auto *sd = p.descriptor_map.GetValueRef(key.c_str());
            sd->set = int(p.set_type);

            auto fit = fixed_binding_map.find(key);
            if(fit != fixed_binding_map.end())
            {
                sd->binding = fit->second;
                continue;
            }

            while(std::find_if(fixed_binding_map.begin(), fixed_binding_map.end(),
                               [&](const auto &pair){ return pair.second == next_binding; }) != fixed_binding_map.end())
                ++next_binding;

            sd->binding = next_binding++;
        }
    }
}
}}//namespace hgl::graph



