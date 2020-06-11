#pragma once
#include<hgl/type/BaseString.h>
#include<hgl/type/List.h>
#include<hgl/type/StringList.h>
#include<hgl/graph/vulkan/VKNamespace.h>

VK_NAMESPACE_BEGIN
    struct ShaderStage
    {
        UTF8String  name;
        uint        location;
        VkFormat    format;
    };//struct ShaderStage

    using ShaderStageList=ObjectList<ShaderStage>;
    using ShaderBindingList=List<uint32_t>;

    struct ShaderDescriptorList
    {
        UTF8StringList      name_list;
        ShaderBindingList   binding_list;
    };

    #ifndef VK_DESCRIPTOR_TYPE_BEGIN_RANGE
    constexpr size_t VK_DESCRIPTOR_TYPE_BEGIN_RANGE=VK_DESCRIPTOR_TYPE_SAMPLER;
    #endif//VK_DESCRIPTOR_TYPE_BEGIN_RANGE

    #ifndef VK_DESCRIPTOR_TYPE_END_RANGE
    constexpr size_t VK_DESCRIPTOR_TYPE_END_RANGE=VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    #endif//VK_DESCRIPTOR_TYPE_END_RANGE
    
    #ifndef VK_DESCRIPTOR_TYPE_RANGE_SIZE
    constexpr size_t VK_DESCRIPTOR_TYPE_RANGE_SIZE=VK_DESCRIPTOR_TYPE_END_RANGE-VK_DESCRIPTOR_TYPE_BEGIN_RANGE+1;
    #endif//VK_DESCRIPTOR_TYPE_RANGE_SIZE

    class ShaderResource
    {
        const void *data;

        VkShaderStageFlagBits stage_flag;

        const void *spv_data;
        uint32 spv_size;

        ShaderStageList stage_inputs;
        ShaderStageList stage_outputs;

        ShaderDescriptorList descriptor_list[VK_DESCRIPTOR_TYPE_RANGE_SIZE];

    public:

        ShaderResource(const void *,const VkShaderStageFlagBits &,const void *,const uint32);
        virtual ~ShaderResource();

        const VkShaderStageFlagBits GetStage()const{return stage_flag;}

        const uint32_t *GetCode     ()const{return (uint32_t *)spv_data;}
        const uint32_t  GetCodeSize ()const{return spv_size;}

        ShaderStageList &GetStageInputs(){return stage_inputs;}
        ShaderStageList &GetStageOutputs(){return stage_outputs;}

        const int GetStageInputBinding(const UTF8String &);

        const ShaderDescriptorList *GetDescriptorList()const{return descriptor_list;}
              ShaderDescriptorList *GetDescriptorList(VkDescriptorType desc_type)
        {
            if(desc_type<VK_DESCRIPTOR_TYPE_BEGIN_RANGE
             ||desc_type>VK_DESCRIPTOR_TYPE_END_RANGE)return nullptr;

            return descriptor_list+desc_type;
        }

        ShaderDescriptorList &GetUBO(){return descriptor_list[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER];}
        ShaderDescriptorList &GetSSBO(){return descriptor_list[VK_DESCRIPTOR_TYPE_STORAGE_BUFFER];}
        ShaderDescriptorList &GetSampler(){return descriptor_list[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER];}

        const int                   GetBinding      (VkDescriptorType desc_type,const UTF8String &name)const;
        const ShaderBindingList *   GetBindingList  (VkDescriptorType desc_type)const
        {
            if(desc_type<VK_DESCRIPTOR_TYPE_BEGIN_RANGE
             ||desc_type>VK_DESCRIPTOR_TYPE_END_RANGE)return nullptr;

            return &(descriptor_list[desc_type].binding_list);
        }
    };//class ShaderResource

    ShaderResource *LoadShaderResoruce(const OSString &filename);
VK_NAMESPACE_END