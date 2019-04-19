#ifndef HGL_GRAPH_VULKAN_PIPELINE_LAYOUT_INCLUDE
#define HGL_GRAPH_VULKAN_PIPELINE_LAYOUT_INCLUDE

#include"VK.h"
#include"VKDescriptorSets.h"
VK_NAMESPACE_BEGIN
class PipelineLayout
{
    VkDevice device;
    VkPipelineLayout layout;

    const DescriptorSets *desc_sets;

private:

    friend PipelineLayout *CreatePipelineLayout(VkDevice dev,const DescriptorSetLayout *dsl);

    PipelineLayout(VkDevice dev,VkPipelineLayout pl,const DescriptorSets *ds){device=dev;layout=pl;desc_sets=ds;}

public:

    ~PipelineLayout();

    operator VkPipelineLayout (){return layout;}

    const uint32_t          GetDescriptorSetCount   ()const{return desc_sets?desc_sets->GetCount():0;}
    const VkDescriptorSet * GetDescriptorSets       ()const{return desc_sets?desc_sets->GetData():nullptr;}
};//class PipelineLayout

PipelineLayout *CreatePipelineLayout(VkDevice dev,const DescriptorSetLayout *dsl);
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_PIPELINE_LAYOUT_INCLUDE
