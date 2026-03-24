#pragma once

#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>
#include <hgl/type/UnorderedMap.h>
#include <map>
#include <vulkan/vulkan.h>
#include <hgl/common/DescriptorSetTypeDef.h>
#include <hgl/mtl/DescriptorBindingContract.h>

namespace hgl::graph
{
    constexpr size_t DESCRIPTOR_NAME_MAX_LENGTH=32;

    struct ShaderDescriptor
    {
        char name[DESCRIPTOR_NAME_MAX_LENGTH];
        VkDescriptorType desc_type;
        DescriptorSetType set_type;

        int set;
        int binding;
        uint32_t stage_flag;

    private:

        void Init()
        {
            mem_zero(name);
            desc_type=VK_DESCRIPTOR_TYPE_MAX_ENUM;
            set_type=DescriptorSetType::Unknow;
            set=-1;
            binding=-1;
            stage_flag=0;
        }

    public:

        ShaderDescriptor()
        {
            Init();
        }

        ShaderDescriptor(const ShaderDescriptor *sr)
        {
            if(!sr)
            {
                Init();
            }
            else
            {
                mem_copy(name,sr->name);
                desc_type   =sr->desc_type;
                set_type    =sr->set_type;
                set         =sr->set;
                binding     =sr->binding;
                stage_flag  =sr->stage_flag;
            }
        }

        virtual ~ShaderDescriptor()=default;

        std::strong_ordering operator<=>(const ShaderDescriptor &sr)const
        {
            if(auto cmp=set<=>sr.set;cmp!=0)
                return cmp;

            if(auto cmp=binding<=>sr.binding;cmp!=0)
                return cmp;

            return hgl::strcmp_ordering(name, sr.name);
        }
    };

    using ShaderDescriptorList=ValueArray<ShaderDescriptor *>;

    struct UBODescriptor:public ShaderDescriptor
    {
        AnsiString type;
        mtl::UBODescriptorSemantic semantic = mtl::UBODescriptorSemantic::Unknown;

    public:

        UBODescriptor()
        {
            desc_type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
    };

    struct SSBODescriptor:public ShaderDescriptor
    {
        AnsiString type;
        mtl::SSBODescriptorSemantic semantic = mtl::SSBODescriptorSemantic::Unknown;

    public:

        SSBODescriptor()
        {
            desc_type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
    };

    struct TextureDescriptor:public ShaderDescriptor
    {
        AnsiString type;
        mtl::SamplerSlot slot = mtl::SamplerSlot::RANGE_SIZE;  ///< For standard texture descriptors mapped to SamplerSlot

    public:

        TextureDescriptor()
        {
            desc_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        }
    };

    struct TextureSamplerDescriptor:public ShaderDescriptor
    {
        AnsiString type;
        mtl::SamplerSlot slot = mtl::SamplerSlot::RANGE_SIZE;  ///< For standard texture descriptors mapped to SamplerSlot

    public:

        TextureSamplerDescriptor()
        {
            desc_type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
    };

    struct ShaderObjectData:public ShaderDescriptor
    {
        AnsiString type;
    };

    struct ConstValueDescriptor
    {
        int constant_id;

        AnsiString type;
        AnsiString name;
        AnsiString value;
    };

    struct SubpassInputDescriptor
    {
        AnsiString name;
        uint8_t input_attachment_index;
        uint8_t binding;
    };

    struct ShaderPushConstant
    {
        AnsiString name;
        uint8_t offset;
        uint8_t size;
    };

    struct ShaderDescriptorSet
    {
        DescriptorSetType set_type;

        int set;
        int count;

        std::map<mtl::UBODescriptorSemantic,UBODescriptor*>    ubo_descriptor_map;
        std::map<mtl::SSBODescriptorSemantic,SSBODescriptor*>  ssbo_descriptor_map;
        std::map<mtl::SamplerSlot,TextureDescriptor*>          texture_descriptor_map;
        std::map<mtl::SamplerSlot,TextureSamplerDescriptor*>   texture_sampler_descriptor_map;

    public:

        UBODescriptor *AddUBO(uint32_t shader_stage_flag_bits,UBODescriptor *new_sd);
        SSBODescriptor *AddSSBO(uint32_t shader_stage_flag_bits,SSBODescriptor *new_sd);
        TextureDescriptor *AddTexture(uint32_t shader_stage_flag_bits,TextureDescriptor *new_sd);
        TextureSamplerDescriptor *AddTextureSampler(uint32_t shader_stage_flag_bits,TextureSamplerDescriptor *new_sd);
    };

    using ShaderDescriptorSetArray=ShaderDescriptorSet[DESCRIPTOR_SET_TYPE_COUNT];
}
