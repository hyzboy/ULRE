#include<hgl/vk/pipeline/VKPipelineLayoutData.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMaterialDescriptorManager.h>

namespace hgl::graph{
PipelineLayoutData *VulkanDevice::CreatePipelineLayoutData(const MaterialDescriptorManager *desc_manager,
                                                               VkDescriptorSetLayout bindless_layout)
{
    PipelineLayoutData *pld=new PipelineLayoutData();  // 使用 new 而不是 hgl_zero_new（因为有析构函数）
    memset(pld, 0, sizeof(PipelineLayoutData));  // 手动清零需要的部分

    if(desc_manager)
    {
        ENUM_CLASS_FOR(DescriptorSetType,int,i)
        {
            // Bindless slot 由外部传入，不走 desc_manager
            if(i==int(DescriptorSetType::Bindless))
                continue;

            const DescriptorSetLayoutCreateInfo *dslci=desc_manager->GetDSLCI((DescriptorSetType)i);

            if(!dslci||dslci->bindingCount<=0)
                continue;

            if(pld->layouts[i])
                vkDestroyDescriptorSetLayout(attr->device,pld->layouts[i],nullptr);

            if(vkCreateDescriptorSetLayout(attr->device,dslci,nullptr,pld->layouts+i)!=VK_SUCCESS)
            {
                delete pld;
                return(nullptr);
            }

            pld->vab_count[i]=dslci->bindingCount;

            pld->fin_dsl[pld->fin_dsl_count]=pld->layouts[i];
            ++pld->fin_dsl_count;
        }

        if(pld->fin_dsl_count<=0)
        {
            delete pld;
            return(nullptr);
        }
    }
    else
    {
        //没有任何DescriptorSet的情况也是存在的
    }

    // 注入 bindless layout（Set 4）
    // 若前面的某些 set 被压缩掉，需要用占位空 layout 补齐，保证 bindless 在正确的 set index
    if(bindless_layout != VK_NULL_HANDLE)
    {
        constexpr int kBindlessIdx = int(DescriptorSetType::Bindless); // = 4

        // 为缺失的中间 set 创建占位空 layout
        while(pld->fin_dsl_count < kBindlessIdx)
        {
            VkDescriptorSetLayoutCreateInfo empty_ci{};
            empty_ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            empty_ci.bindingCount = 0;

            VkDescriptorSetLayout empty_layout = VK_NULL_HANDLE;
            if(vkCreateDescriptorSetLayout(attr->device, &empty_ci, nullptr, &empty_layout) != VK_SUCCESS)
            {
                delete pld;
                return nullptr;
            }

            pld->placeholder_layouts.push_back(empty_layout);
            pld->fin_dsl[pld->fin_dsl_count++] = empty_layout;
        }

        pld->fin_dsl[pld->fin_dsl_count] = bindless_layout;
        pld->bindless_set_index = pld->fin_dsl_count;
        ++pld->fin_dsl_count;
    }
    else
    {
        pld->bindless_set_index = -1;
    }

    //VkPushConstantRange push_constant_range;

    //push_constant_range.stageFlags   = VK_SHADER_STAGE_VERTEX_BIT;
    //push_constant_range.size         = MAX_PUSH_CONSTANT_BYTES;
    //push_constant_range.offset       = 0;

    PipelineLayoutCreateInfo pPipelineLayoutCreateInfo;

    pPipelineLayoutCreateInfo.setLayoutCount            = pld->fin_dsl_count;
    pPipelineLayoutCreateInfo.pSetLayouts               = pld->fin_dsl;
    pPipelineLayoutCreateInfo.pushConstantRangeCount    = 0;//1;
    pPipelineLayoutCreateInfo.pPushConstantRanges       = nullptr;//&push_constant_range;

    pld->device=attr->device;

    if(vkCreatePipelineLayout(attr->device,&pPipelineLayoutCreateInfo,nullptr,&(pld->pipeline_layout))!=VK_SUCCESS)
    {
        delete pld;
        return(nullptr);
    }

    return(pld);
}

PipelineLayoutData::~PipelineLayoutData()
{
    vkDestroyPipelineLayout(device,pipeline_layout,nullptr);

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
        if(layouts[i])
            vkDestroyDescriptorSetLayout(device,layouts[i],nullptr);

    // 释放 bindless 占位空 layouts
    for(auto pl : placeholder_layouts)
        if(pl != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(device, pl, nullptr);
}
}//namespace hgl::graph

