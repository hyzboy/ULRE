#include"VKShader.h"
#include"spirv_cross.hpp"

VK_NAMESPACE_BEGIN

void shader_dump(const void *spv_data,const uint32_t spv_size)
{
    spirv_cross::Compiler comp((const uint32_t *)spv_data,spv_size/sizeof(uint32_t));

    spirv_cross::ShaderResources res=comp.get_shader_resources();

    for(auto &ref:res.sampled_images)
    {
        unsigned set=comp.get_decoration(ref.id,spv::DecorationDescriptorSet);
        unsigned binding=comp.get_decoration(ref.id,spv::DecorationBinding);
        const std::string name=comp.get_name(ref.id);

        std::cout<<"sampled image ["<<ref.id<<":"<<name.c_str()<<"] set="<<set<<",binding="<<binding<<std::endl;
    }

    for(auto &ref:res.stage_inputs)
    {
        unsigned set=comp.get_decoration(ref.id,spv::DecorationDescriptorSet);
        unsigned location=comp.get_decoration(ref.id,spv::DecorationLocation);
        const std::string name=comp.get_name(ref.id);

        std::cout<<"stage input ["<<ref.id<<":"<<name.c_str()<<"] set="<<set<<",location="<<location<<std::endl;
    }

    for(auto &ref:res.uniform_buffers)
    {
        unsigned set=comp.get_decoration(ref.id,spv::DecorationDescriptorSet);
        unsigned binding=comp.get_decoration(ref.id,spv::DecorationBinding);
        const std::string name=comp.get_name(ref.id);

        std::cout<<"UBO ["<<ref.id<<":"<<name.c_str()<<"] set="<<set<<",binding="<<binding<<std::endl;
    }

    for(auto &ref:res.stage_outputs)
    {
        unsigned set=comp.get_decoration(ref.id,spv::DecorationDescriptorSet);
        unsigned location=comp.get_decoration(ref.id,spv::DecorationLocation);
        const std::string name=comp.get_name(ref.id);

        std::cout<<"stage output ["<<ref.id<<":"<<name.c_str()<<"] set="<<set<<",location="<<location<<std::endl;
    }
}

Shader::~Shader()
{
    const int count=shader_stage_list.GetCount();

    if(count>0)
    {
        VkPipelineShaderStageCreateInfo *ss=shader_stage_list.GetData();
        for(int i=0;i<count;i++)
        {
            vkDestroyShaderModule(device,ss->module,nullptr);
            ++ss;
        }
    }
}

bool Shader::Add(const VkShaderStageFlagBits shader_stage_bit,const void *spv_data,const uint32_t spv_size)
{   
    shader_dump(spv_data,spv_size);

    VkPipelineShaderStageCreateInfo shader_stage;
    shader_stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage.pNext=nullptr;
    shader_stage.pSpecializationInfo=nullptr;
    shader_stage.flags=0;
    shader_stage.stage=shader_stage_bit;
    shader_stage.pName="main";

    VkShaderModuleCreateInfo moduleCreateInfo;
    moduleCreateInfo.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.pNext=nullptr;
    moduleCreateInfo.flags=0;
    moduleCreateInfo.codeSize=spv_size;
    moduleCreateInfo.pCode=(const uint32_t *)spv_data;

    if(vkCreateShaderModule(device,&moduleCreateInfo,nullptr,&shader_stage.module)!=VK_SUCCESS)
        return(false);

    shader_stage_list.Add(shader_stage);

    return(true);
}
VK_NAMESPACE_END
