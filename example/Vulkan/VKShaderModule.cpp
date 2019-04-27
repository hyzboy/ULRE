#include"VKShaderModule.h"

VK_NAMESPACE_BEGIN
ShaderModule::ShaderModule(int id,VkPipelineShaderStageCreateInfo *sci)
{
    shader_id=id;
    ref_count=0;

    stage_create_info=sci;
}

ShaderModule::~ShaderModule()
{
    delete stage_create_info;
}
VK_NAMESPACE_END
