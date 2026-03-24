#pragma once

#include <hgl/common/ShaderDescriptorDef.h>
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

    bool ubo_struct_by_semantic [mtl::UBODescriptorSemanticCount] = {};
    bool ssbo_struct_by_semantic[mtl::SSBODescriptorSemanticCount] = {};
    ankerl::unordered_dense::map<mtl::UBODescriptorSemantic,UBODescriptor *> ubo_map;
    ankerl::unordered_dense::map<mtl::SSBODescriptorSemantic,SSBODescriptor *> ssbo_map;

    TextureDescriptor        *texture_by_slot        [mtl::SamplerSlotCount] = {};
    TextureSamplerDescriptor *texture_sampler_by_slot[mtl::SamplerSlotCount] = {};

    ankerl::unordered_dense::map<std::string,TextureDescriptor *> texture_map;
    ankerl::unordered_dense::map<std::string,TextureSamplerDescriptor *> texture_sampler_map;

public:

    MaterialDescriptorInfo();
    ~MaterialDescriptorInfo()=default;

    bool AddUBOStruct(const mtl::UBODescriptorSemantic semantic)
    {
        if(!mtl::IsBuiltinDescriptorSemantic(semantic))
            return false;

        ubo_struct_by_semantic[size_t(semantic)] = true;
        return true;
    }

    bool AddSSBOStruct(const mtl::SSBODescriptorSemantic semantic)
    {
        if(!mtl::IsBuiltinDescriptorSemantic(semantic))
            return false;

        ssbo_struct_by_semantic[size_t(semantic)] = true;
        return true;
    }

    bool hasUBOStruct(const mtl::UBODescriptorSemantic semantic) const
    {
        if(!mtl::IsBuiltinDescriptorSemantic(semantic))
            return false;

        return ubo_struct_by_semantic[size_t(semantic)];
    }

    bool hasSSBOStruct(const mtl::SSBODescriptorSemantic semantic) const
    {
        if(!mtl::IsBuiltinDescriptorSemantic(semantic))
            return false;

        return ssbo_struct_by_semantic[size_t(semantic)];
    }

    const UBODescriptor *AddUBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,UBODescriptor *sd);
    const SSBODescriptor *AddSSBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,SSBODescriptor *sd);
    const TextureDescriptor *AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureDescriptor *sd);
    const TextureSamplerDescriptor *AddTextureSampler(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureSamplerDescriptor *sd);

    TextureDescriptor *GetTexture(const std::string &name);
    TextureSamplerDescriptor *GetTextureSampler(const std::string &name);
    TextureDescriptor *GetTexture(mtl::SamplerSlot slot);
    TextureSamplerDescriptor *GetTextureSampler(mtl::SamplerSlot slot);
    TextureDescriptor *GetTexture(const char *name){return GetTexture(std::string(name?name:""));}
    TextureSamplerDescriptor *GetTextureSampler(const char *name){return GetTextureSampler(std::string(name?name:""));}

    UBODescriptor  *GetUBO (mtl::UBODescriptorSemantic semantic);
    SSBODescriptor *GetSSBO(mtl::SSBODescriptorSemantic semantic);

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
