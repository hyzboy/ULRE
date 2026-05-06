#pragma once

#include <hgl/common/ShaderDescriptorDef.h>
#include <hgl/mtl/SamplerSlot.h>
#include<string>
#include<memory>

namespace hgl{namespace graph{
/**
* 材质描述符管理
* 该类使用于SHADER生成前，用于统计编号set/binding
*/
class MaterialDescriptorDB
{
    uint32_t descriptor_count;
    ShaderDescriptorSetArray desc_set_array;

    bool ubo_struct_by_semantic [mtl::UBODescriptorSemanticCount] = {};
    bool ssbo_struct_by_semantic[mtl::SSBODescriptorSemanticCount] = {};
    UBODescriptor  *ubo_by_semantic [mtl::UBODescriptorSemanticCount] = {};
    SSBODescriptor *ssbo_by_semantic[mtl::SSBODescriptorSemanticCount] = {};

    TextureDescriptor        *texture_by_slot        [mtl::SamplerSlotCount] = {};
    TextureSamplerDescriptor *texture_sampler_by_slot[mtl::SamplerSlotCount] = {};

public:

    MaterialDescriptorDB();
    ~MaterialDescriptorDB();

    bool AddUBOStruct(const mtl::UBODescriptorSemantic semantic)
    {
        if(!RangeCheck(semantic))
            return false;

        ubo_struct_by_semantic[size_t(semantic)] = true;
        return true;
    }

    bool AddSSBOStruct(const mtl::SSBODescriptorSemantic semantic)
    {
        if(!RangeCheck(semantic))
            return false;

        ssbo_struct_by_semantic[size_t(semantic)] = true;
        return true;
    }

    bool hasUBOStruct(const mtl::UBODescriptorSemantic semantic) const
    {
        if(!RangeCheck(semantic))
            return false;

        return ubo_struct_by_semantic[size_t(semantic)];
    }

    bool hasSSBOStruct(const mtl::SSBODescriptorSemantic semantic) const
    {
        if(!RangeCheck(semantic))
            return false;

        return ssbo_struct_by_semantic[size_t(semantic)];
    }

    const UBODescriptor *AddUBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,UBODescriptor *sd);
    const UBODescriptor *AddUBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,std::unique_ptr<UBODescriptor> sd);

    const SSBODescriptor *AddSSBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,SSBODescriptor *sd);
    const SSBODescriptor *AddSSBO(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,std::unique_ptr<SSBODescriptor> sd);

    const TextureDescriptor *AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureDescriptor *sd);
    const TextureDescriptor *AddTexture(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,std::unique_ptr<TextureDescriptor> sd);

    const TextureSamplerDescriptor *AddTextureSampler(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,TextureSamplerDescriptor *sd);
    const TextureSamplerDescriptor *AddTextureSampler(uint32_t shader_stage_flag_bits,DescriptorSetType set_type,std::unique_ptr<TextureSamplerDescriptor> sd);

    TextureDescriptor *GetTexture(mtl::SamplerSlot slot)
    {
        const size_t index = size_t(slot);
        return index < mtl::SamplerSlotCount ? texture_by_slot[index] : nullptr;
    }
    const TextureDescriptor *GetTexture(mtl::SamplerSlot slot) const
    {
        const size_t index = size_t(slot);
        return index < mtl::SamplerSlotCount ? texture_by_slot[index] : nullptr;
    }
    TextureSamplerDescriptor *GetTextureSampler(mtl::SamplerSlot slot)
    {
        const size_t index = size_t(slot);
        return index < mtl::SamplerSlotCount ? texture_sampler_by_slot[index] : nullptr;
    }
    const TextureSamplerDescriptor *GetTextureSampler(mtl::SamplerSlot slot) const
    {
        const size_t index = size_t(slot);
        return index < mtl::SamplerSlotCount ? texture_sampler_by_slot[index] : nullptr;
    }

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
};//class MaterialDescriptorDB
}}//namespace hgl::graph
