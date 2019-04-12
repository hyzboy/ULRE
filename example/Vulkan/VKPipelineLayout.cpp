#include"VKPipelineLayout.h"

VK_NAMESPACE_BEGIN
PipelineLayout::~PipelineLayout()
{
    if(!layout)return;

    const int count=desc_set_layout.GetCount();

    if(count>0)
    {
        VkDescriptorSetLayout *dsl=desc_set_layout.GetData();

        for(int i=0;i<count;i++)
        {
            vkDestroyDescriptorSetLayout(device,*dsl,nullptr);
            ++dsl;
        }
    }

    vkDestroyPipelineLayout(device,layout,nullptr);
}
VK_NAMESPACE_END
