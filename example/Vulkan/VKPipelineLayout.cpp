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
    if(!dsl)return(nullptr);

    if(dsl->GetCount()<=0)return(nullptr);

    DescriptorSets *desc_sets=dsl->CreateSets();

    if(!desc_sets)
        return(nullptr);

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = nullptr;
    pPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    pPipelineLayoutCreateInfo.setLayoutCount = dsl->GetCount();
    pPipelineLayoutCreateInfo.pSetLayouts = dsl->GetData();

    VkPipelineLayout pipeline_layout;

    if(vkCreatePipelineLayout(dev, &pPipelineLayoutCreateInfo, nullptr, &pipeline_layout)!=VK_SUCCESS)
    {
        delete desc_sets;
        return(nullptr);
    }

    return(new PipelineLayout(dev,pipeline_layout,desc_sets));
}
VK_NAMESPACE_END
