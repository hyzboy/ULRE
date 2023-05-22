#pragma once

#include<hgl/graph/VKShaderDescriptorSet.h>
#include<hgl/type/Map.h>

namespace hgl{namespace graph{
/**
* 材质描述符管理</p>
* 该类使用于SHADER生成前，用于统计编号set/binding
*/
class MaterialDescriptorInfo
{
    uint descriptor_count;
    ShaderDescriptorSetArray desc_set_array;

    Map<AnsiString,AnsiString> struct_map;
    Map<AnsiString,UBODescriptor *> ubo_map;
    Map<AnsiString,SamplerDescriptor *> sampler_map;

public:
    
    MaterialDescriptorInfo();
    ~MaterialDescriptorInfo()=default;

    bool AddStruct(const AnsiString &name,const AnsiString &code)
    {
        struct_map.Add(name,code);
        return(true);
    }

    bool GetStruct(const AnsiString &name,AnsiString &code)
    {
        return(struct_map.Get(name,code));
    }

    bool hasStruct(const AnsiString &name) const
    {
        return(struct_map.KeyExist(name));
    }

    const UBODescriptor *AddUBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,UBODescriptor *sd);
    const SamplerDescriptor *AddSampler(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,SamplerDescriptor *sd);

    UBODescriptor *GetUBO(const AnsiString &name);
    SamplerDescriptor *GetSampler(const AnsiString &name);

    const DescriptorSetType GetSetType(const AnsiString &)const;

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
