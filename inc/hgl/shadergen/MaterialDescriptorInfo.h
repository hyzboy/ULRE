#pragma once

#include <hgl/common/ShaderDescriptorDef.h>
#include<hgl/mtl/ShaderBufferSource.h>
#include<ankerl/unordered_dense.h>
#include<string>

namespace hgl{namespace graph{
/**
* 材质描述符管理</p>
* 该类使用于SHADER生成前，用于统计编号set/binding
*/
class MaterialDescriptorInfo
{
    uint descriptor_count;
    ShaderDescriptorSetArray desc_set_array;

    ankerl::unordered_dense::map<std::string,std::string> struct_map;
    ankerl::unordered_dense::map<std::string,UBODescriptor *> ubo_map;
    ankerl::unordered_dense::map<std::string,SSBODescriptor *> ssbo_map;
    ankerl::unordered_dense::map<std::string,TextureDescriptor *> texture_map;
    ankerl::unordered_dense::map<std::string,TextureSamplerDescriptor *> texture_sampler_map;

private:

    static std::string KeyFrom(const std::string &text)
    {
        return text;
    }

    static std::string KeyFrom(const char *text)
    {
        return std::string(text?text:"");
    }

public:

    MaterialDescriptorInfo();
    ~MaterialDescriptorInfo()=default;

    bool AddStruct(const std::string &name,const std::string &code)
    {
        struct_map[KeyFrom(name)] = KeyFrom(code);
        return(true);
    }
    bool AddStruct(const char *name,const char *code)
    {
        return AddStruct(KeyFrom(name),KeyFrom(code));
    }
    bool AddStruct(const char *name,const std::string &code)
    {
        return AddStruct(KeyFrom(name),code);
    }
    bool AddStruct(const std::string &name,const char *code)
    {
        return AddStruct(name,KeyFrom(code));
    }

    bool AddStruct(const ShaderBufferSource &ss)
    {
        return(AddStruct(ss.struct_name,""));
    }

    bool GetStruct(const std::string &name,std::string &code)
    {
        const auto iter=struct_map.find(name);
        if(iter==struct_map.end())
            return false;

        code=iter->second;
        return true;
    }

    bool hasStruct(const std::string &name) const
    {
        return struct_map.contains(name);
    }
    bool hasStruct(const char *name) const
    {
        return struct_map.contains(KeyFrom(name));
    }

    const UBODescriptor *AddUBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,UBODescriptor *sd);
    const SSBODescriptor *AddSSBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,SSBODescriptor *sd);
    const TextureDescriptor *AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureDescriptor *sd);
    const TextureSamplerDescriptor *AddTextureSampler(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureSamplerDescriptor *sd);

    UBODescriptor *GetUBO(const std::string &name);
    SSBODescriptor *GetSSBO(const std::string &name);
    TextureDescriptor *GetTexture(const std::string &name);
    TextureSamplerDescriptor *GetTextureSampler(const std::string &name);
    UBODescriptor *GetUBO(const char *name){return GetUBO(std::string(name?name:""));}
    SSBODescriptor *GetSSBO(const char *name){return GetSSBO(std::string(name?name:""));}
    TextureDescriptor *GetTexture(const char *name){return GetTexture(std::string(name?name:""));}
    TextureSamplerDescriptor *GetTextureSampler(const char *name){return GetTextureSampler(std::string(name?name:""));}

    const DescriptorSetType GetSetType(const std::string &name)const;
    const DescriptorSetType GetSetType(const char *name)const
    {
        return GetSetType(std::string(name?name:""));
    }

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
