#pragma once

#include <vulkan/vulkan.h>
#include <hgl/common/DescriptorSetTypeDef.h>
#include <hgl/common/AttributeProvider.h>

#include <hgl/mtl/DescriptorSemanticRegistry.h>
#include <cctype>

namespace hgl::graph
{
    constexpr size_t DESCRIPTOR_NAME_MAX_LENGTH=32;

    // VertexStreams uses VertexAttrib-ordinal SSBO bindings.
    constexpr size_t VERTEX_STREAM_SSBO_BINDING_COUNT = size_t(uint32_t(VertexAttrib::RANGE_SIZE));
    constexpr size_t SHADER_DESCRIPTOR_SSBO_SLOT_COUNT =
        (mtl::SSBODescriptorSemanticCount > VERTEX_STREAM_SSBO_BINDING_COUNT)
            ? mtl::SSBODescriptorSemanticCount
            : VERTEX_STREAM_SSBO_BINDING_COUNT;

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

        virtual std::string GetBindingMacroName() const
        {
            if (!name || !name[0]) return {};
            std::string result;
            for (const char *p = name; *p; ++p)
                result += static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
            result += "_BINDING";
            return result;
        }

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
        std::string type;
        mtl::UBODescriptorSemantic semantic = mtl::UBODescriptorSemantic::Unknown;

    public:

        UBODescriptor()
        {
            desc_type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }

        std::string GetBindingMacroName() const override
        {
            if (semantic != mtl::UBODescriptorSemantic::Unknown)
            {
                const auto &meta = mtl::GetDescriptorSemanticMeta(semantic);
                if (meta.binding_macro_name && *meta.binding_macro_name)
                    return meta.binding_macro_name;
            }
            return ShaderDescriptor::GetBindingMacroName();
        }
    };

    struct SSBODescriptor:public ShaderDescriptor
    {
        std::string type;
        mtl::SSBODescriptorSemantic semantic = mtl::SSBODescriptorSemantic::Unknown;

    public:

        SSBODescriptor()
        {
            desc_type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }

        std::string GetBindingMacroName() const override
        {
            if (semantic != mtl::SSBODescriptorSemantic::Unknown)
            {
                const auto &meta = mtl::GetDescriptorSemanticMeta(semantic);
                if (meta.binding_macro_name && *meta.binding_macro_name)
                    return meta.binding_macro_name;
            }
            return ShaderDescriptor::GetBindingMacroName();
        }
    };

    struct TextureDescriptor:public ShaderDescriptor
    {
        std::string type;
        mtl::SamplerSlot slot = mtl::SamplerSlot::RANGE_SIZE;  ///< For standard texture descriptors mapped to SamplerSlot
        TextureChannelHint channel_hint = TextureChannelHint::RGBA;

    public:

        TextureDescriptor()
        {
            desc_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        }

        std::string GetBindingMacroName() const override
        {
            if (slot != mtl::SamplerSlot::RANGE_SIZE)
                return mtl::ToBindingMacroName(slot);
            return ShaderDescriptor::GetBindingMacroName();
        }
    };

    struct TextureSamplerDescriptor:public ShaderDescriptor
    {
        std::string type;
        mtl::SamplerSlot slot = mtl::SamplerSlot::RANGE_SIZE;  ///< For standard texture descriptors mapped to SamplerSlot
        TextureChannelHint channel_hint = TextureChannelHint::RGBA;

    public:

        TextureSamplerDescriptor()
        {
            desc_type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }

        std::string GetBindingMacroName() const override
        {
            if (slot != mtl::SamplerSlot::RANGE_SIZE)
                return mtl::ToBindingMacroName(slot);
            return ShaderDescriptor::GetBindingMacroName();
        }
    };

    struct ShaderObjectData:public ShaderDescriptor
    {
        std::string type;
    };

    struct ConstValueDescriptor
    {
        int constant_id;

        std::string type;
        std::string name;
        std::string value;
    };

    struct ShaderPushConstant
    {
        std::string name;
        uint8_t offset;
        uint8_t size;
    };

    struct ShaderDescriptorSet
    {
        DescriptorSetType set_type;

        int set;
        int count;

        UBODescriptor           *ubo_descriptor_map             [mtl::UBODescriptorSemanticCount]  = {};
        SSBODescriptor          *ssbo_descriptor_map            [SHADER_DESCRIPTOR_SSBO_SLOT_COUNT] = {};
        TextureDescriptor       *texture_descriptor_map         [mtl::SamplerSlotCount]            = {};
        TextureSamplerDescriptor *texture_sampler_descriptor_map[mtl::SamplerSlotCount]            = {};

    public:

        UBODescriptor *AddUBO(uint32_t shader_stage_flag_bits,UBODescriptor *new_sd);
        SSBODescriptor *AddSSBO(uint32_t shader_stage_flag_bits,SSBODescriptor *new_sd);
        SSBODescriptor *AddVertexStreamSSBO(uint32_t shader_stage_flag_bits,uint32_t binding,SSBODescriptor *new_sd);
        TextureDescriptor *AddTexture(uint32_t shader_stage_flag_bits,TextureDescriptor *new_sd);
        TextureSamplerDescriptor *AddTextureSampler(uint32_t shader_stage_flag_bits,TextureSamplerDescriptor *new_sd);
    };

    using ShaderDescriptorSetArray=ShaderDescriptorSet[DESCRIPTOR_SET_TYPE_COUNT];
}
