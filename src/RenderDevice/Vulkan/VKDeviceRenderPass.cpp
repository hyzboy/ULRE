#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKRenderPass.h>

VK_NAMESPACE_BEGIN
void Device::CreateSubpassDependency(List<VkSubpassDependency> &subpass_dependency_list,const uint32_t count)
{
    if(count==1)
    {
        VkSubpassDependency dependency;
        
        dependency.srcSubpass        = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass        = 0;
        dependency.srcStageMask      = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependency.dstStageMask      = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependency.srcAccessMask     = VK_ACCESS_MEMORY_READ_BIT;
        dependency.dstAccessMask     = VK_ACCESS_MEMORY_READ_BIT;
        dependency.dependencyFlags   = VK_DEPENDENCY_BY_REGION_BIT;

        subpass_dependency_list.Add(dependency);
    }
    else
    {
        for(uint32_t i=0;i<count;i++)
        {        
            VkSubpassDependency dependency;

            if(i==0)
            {
                dependency.srcSubpass        = VK_SUBPASS_EXTERNAL;
                dependency.dstSubpass        = 0;
                dependency.srcStageMask      = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;            
                dependency.dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency.srcAccessMask     = VK_ACCESS_MEMORY_READ_BIT;
                dependency.dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            }
            else
            {
                dependency.srcSubpass        = i-1;
                dependency.srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

                if(i==count-1)
                {
                    dependency.dstSubpass        = VK_SUBPASS_EXTERNAL;
                    dependency.dstStageMask      = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                    dependency.srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    dependency.dstAccessMask     = VK_ACCESS_MEMORY_READ_BIT;
                }
                else
                {
                    dependency.dstSubpass        = i;
                    dependency.dstStageMask      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    dependency.srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    dependency.dstAccessMask     = VK_ACCESS_SHADER_READ_BIT;
                }
            }

            dependency.dependencyFlags  = VK_DEPENDENCY_BY_REGION_BIT;
        
            subpass_dependency_list.Add(dependency);
        }
    }
}

bool Device::CreateAttachment(List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const List<VkFormat> &color_format,const VkFormat depth_format,VkImageLayout color_final_layout,VkImageLayout depth_final_layout)
{
    if(!attr->physical_device->CheckDepthFormat(depth_format))
        return(false);

    uint atta_count=color_format.GetCount();

    desc_list.SetCount(atta_count+1);
    VkAttachmentDescription *desc=desc_list.GetData();

    ref_list.SetCount(atta_count+1);
    VkAttachmentReference *ref=ref_list.GetData();

    for(uint i=0;i<atta_count+1;i++)
    {
        desc[i].flags           = 0;
        desc[i].samples         = VK_SAMPLE_COUNT_1_BIT;
        desc[i].loadOp          = VK_ATTACHMENT_LOAD_OP_CLEAR;      //LOAD_OP_CLEAR代表LOAD时清空内容
        desc[i].storeOp         = VK_ATTACHMENT_STORE_OP_STORE;     //STORE_OP_STROE代表SOTRE时储存内容
        desc[i].stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  //DONT CARE表示不在意
        desc[i].stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc[i].initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;        //代表不关心初始布局
    }

    const VkFormat *cf=color_format.GetData();
    for(int i=0;i<atta_count;i++)
    {
        desc[i].finalLayout      = color_final_layout;
        desc[i].format           = *cf;
        ++cf;

        ref[i].attachment       = i;
        ref[i].layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    desc[atta_count].finalLayout  = depth_final_layout;
    desc[atta_count].format       = depth_format;
    
    ref[atta_count].attachment  = atta_count;
    ref[atta_count].layout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    return(true);
}

RenderPass *Device::CreateRenderPass(   const List<VkAttachmentDescription> &desc_list,
                                        const List<VkSubpassDescription> &subpass,
                                        const List<VkSubpassDependency> &dependency,
                                        const List<VkFormat> &color_format,
                                        const VkFormat depth_format,
                                        const VkImageLayout color_final_layout,
                                        const VkImageLayout depth_final_layout)
{

    VkRenderPassCreateInfo rp_info;
    rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext           = nullptr;
    rp_info.attachmentCount = desc_list.GetCount();
    rp_info.pAttachments    = desc_list.GetData();
    rp_info.subpassCount    = subpass.GetCount();
    rp_info.pSubpasses      = subpass.GetData();
    rp_info.dependencyCount = dependency.GetCount();
    rp_info.pDependencies   = dependency.GetData();

    VkRenderPass render_pass;

    if(vkCreateRenderPass(attr->device,&rp_info,nullptr,&render_pass)!=VK_SUCCESS)
        return(nullptr);

    return(new RenderPass(attr->device,render_pass,color_format,depth_format));
}

RenderPass *Device::CreateRenderPass(VkFormat color_format,VkFormat depth_format,VkImageLayout color_final_layout,VkImageLayout depth_final_layout)
{
    List<VkAttachmentReference> ref_list;
    List<VkAttachmentDescription> desc_list;

    List<VkFormat> color_format_list;

    color_format_list.Add(color_format);

    CreateAttachment(ref_list,desc_list,color_format_list,depth_format,color_final_layout,depth_final_layout);

    List<VkSubpassDescription> subpass_desc_list;

    VkSubpassDescription subpass;
        
    subpass.flags                   = 0;
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount    = 0;
    subpass.pInputAttachments       = nullptr;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = ref_list.GetData();
    subpass.pResolveAttachments     = nullptr;
    subpass.pDepthStencilAttachment = ref_list.GetData()+1;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments    = nullptr;

    subpass_desc_list.Add(subpass);

    List<VkSubpassDependency> subpass_dependency_list;

    CreateSubpassDependency(subpass_dependency_list,1);

    return CreateRenderPass(desc_list,subpass_desc_list,subpass_dependency_list,color_format_list,depth_format,color_final_layout,depth_final_layout);
}
VK_NAMESPACE_END
