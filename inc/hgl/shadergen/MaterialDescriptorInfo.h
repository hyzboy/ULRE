#include <string>
#include <unordered_map>
#pragma once

#include<hgl/vk/VKShaderDescriptorSet.h>
#include<hgl/graph/mtl/ShaderBufferSource.h>

namespace hgl{namespace graph{
/**
* 材质描述符管理</p>
* 该类使用于SHADER生成前，用于统计编号set/binding
*/
class MaterialDescriptorInfo
{
    uint descriptor_count;
    ShaderDescriptorSetArray desc_set_array;

    std::unordered_map<std::string, std::string> struct_map;
    std::unordered_map<std::string, UBODescriptor *> ubo_map;
    std::unordered_map<std::string, SSBODescriptor *> ssbo_map;
    std::unordered_map<std::string, TextureDescriptor *> texture_map;
    std::unordered_map<std::string, TextureSamplerDescriptor *> texture_sampler_map;

public:

    MaterialDescriptorInfo();
    ~MaterialDescriptorInfo()=default;

    bool AddStruct(const std::string &name,const std::string &code)
    {
           struct_map[name] = code;
        return(true);
    }

    bool AddStruct(const ShaderBufferSource &ss)
    {
        return(AddStruct(ss.struct_name,ss.codes));
    }

    bool GetStruct(const std::string &name,std::string &code)
    {
        auto it = struct_map.find(name);
        if(it != struct_map.end())
        {
            code = it->second;
            return true;
        }
        return false;
    }

    bool hasStruct(const std::string &name) const
    {
        return struct_map.count(name) > 0;
    }

    const UBODescriptor *AddUBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,UBODescriptor *sd);
    const SSBODescriptor *AddSSBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,SSBODescriptor *sd);
    const TextureDescriptor *AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureDescriptor *sd);
    const TextureSamplerDescriptor *AddTextureSampler(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureSamplerDescriptor *sd);

    UBODescriptor *GetUBO(const std::string &name);
    SSBODescriptor *GetSSBO(const std::string &name);
    TextureDescriptor *GetTexture(const std::string &name);
    TextureSamplerDescriptor *GetTextureSampler(const std::string &name);

    const DescriptorSetType GetSetType(const std::string &)const;

    void Resort();      //排序产生set号与binding号

    const uint GetCount()const
    {
        return descriptor_count;
    }

    const ShaderDescriptorSetArray &Get()const
    {
        return desc_set_array;
    }

    const bool hasSet(const DescriptorSetType &type)const
    {
        return desc_set_array[size_t(type)].count>0;
    }
};//class MaterialDescriptorInfo
}}//namespace hgl::graph
