#pragma once
#include<hgl/type/String.h>
#include<hgl/type/ArrayList.h>
#include<hgl/graph/VKDescriptorSetType.h>

namespace hgl
{
    namespace graph
    {
        constexpr size_t DESCRIPTOR_NAME_MAX_LENGTH=32;

        struct ShaderDescriptor:public Comparator<ShaderDescriptor>
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
                set_type=DescriptorSetType::Global;
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
                    Init();     ////注：请不要使用memset(this,0..)，因为这会破坏Comparator<>纯虚函数表
                }
                else
                {       //注：请不要使用memcpy/mem_copy(*this,sr)来复制数据，因为这会破坏Comparator<>纯虚函数表
                    mem_copy(name,sr->name);
                    desc_type   =sr->desc_type;
                    set_type    =sr->set_type;
                    set         =sr->set;
                    binding     =sr->binding;
                    stage_flag  =sr->stage_flag;
                }
            }

            virtual ~ShaderDescriptor()=default;

            const int compare(const ShaderDescriptor &sr)const override
            {
                if(set!=sr.set)return sr.set-set;
                if(binding!=sr.binding)return sr.binding-binding;

                return strcmp(name,sr.name);
            }
        };//struct ShaderDescriptor

        using ShaderDescriptorList=ArrayList<ShaderDescriptor *>;

        struct UBODescriptor:public ShaderDescriptor
        {
            AnsiString type;

        public:

            UBODescriptor()
            {
                desc_type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            }
        };

        struct TextureDescriptor:public ShaderDescriptor
        {
            AnsiString type;

        public:

            TextureDescriptor()
            {
                desc_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            }
        };

        struct TextureSamplerDescriptor:public ShaderDescriptor
        {
            AnsiString type;

        public:

            TextureSamplerDescriptor()
            {
                desc_type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            }
        };

        /**
         * 未归类的描述符对象，暂没想好怎么命名
         */
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
    }//namespace graph
}//namespace hgl
