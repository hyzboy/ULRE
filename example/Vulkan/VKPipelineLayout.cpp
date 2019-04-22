#include"VKPipelineLayout.h"
#include"VKDescriptorSets.h"

VK_NAMESPACE_BEGIN
PipelineLayout::~PipelineLayout()
{
    if(layout)
        vkDestroyPipelineLayout(device,layout,nullptr);
}

PipelineLayout *CreatePipelineLayout(VkDevice dev,const DescriptorSetLayout *dsl)
{
    const uint32_t layout_count=(dsl?dsl->GetCount():0);
    const VkDescriptorSetLayout *layouts=(layout_count>0?dsl->GetLayouts():nullptr);

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = nullptr;
    pPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    pPipelineLayoutCreateInfo.setLayoutCount = layout_count;
    pPipelineLayoutCreateInfo.pSetLayouts = layouts;

    VkPipelineLayout pipeline_layout;

    if(vkCreatePipelineLayout(dev, &pPipelineLayoutCreateInfo, nullptr, &pipeline_layout)!=VK_SUCCESS)
        return(nullptr);

    return(new PipelineLayout(dev,pipeline_layout,dsl->GetSets()));
}
VK_NAMESPACE_END
