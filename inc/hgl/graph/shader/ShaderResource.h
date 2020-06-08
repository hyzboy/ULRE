#pragma once
#include<hgl/type/BaseString.h>
#include<hgl/type/List.h>
#include<vulkan/vulkan.h>

namespace hgl
{
    namespace graph
    {
        struct ShaderStage
        {
            UTF8String  name;
            uint        location;
            VkFormat    format;
        };//struct ShaderStage

        using ShaderStageList=ObjectList<ShaderStage>;

        struct ShaderDescriptor
        {
            UTF8String  name;
            uint        binding;
        };//struct ShaderDescriptor

        using ShaderDescriptorList=ObjectList<ShaderDescriptor>;

        class ShaderResource
        {
            ShaderStageList is_list;
            ShaderStageList os_list;

            ShaderDescriptorList ubo_list;
            ShaderDescriptorList sampler_list;

        public:

            ShaderStageList &GetInputStages(){return is_list;}
            ShaderStageList &GetOutputStages(){return os_list;}

            ShaderDescriptorList &GetUBO(){return ubo_list;}
            ShaderDescriptorList &GetSampler(){return sampler_list;}
        };//class ShaderResource

        ShaderResource *LoadShaderResoruce(const OSString &filename);
    }//namespace graph
}//namespace hgl
