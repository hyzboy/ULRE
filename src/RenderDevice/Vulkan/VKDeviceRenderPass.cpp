#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKRenderPass.h>

VK_NAMESPACE_BEGIN
RenderPass *Device::CreateRenderPass(List<VkSubpassDescription> &subpass,List<VkFormat> color_format,VkFormat depth_format,VkImageLayout color_final_layout,VkImageLayout depth_final_layout)
{
    int depth=-1;
    uint atta_count=color_format.GetCount();

    if(attr->physical_device->CheckDepthFormat(depth_format))
    {
        depth=atta_count++;
    }

    SharedArray<VkAttachmentDescription> attachments=new VkAttachmentDescription[atta_count];
    SharedArray<VkAttachmentReference> color_references=new VkAttachmentReference[color_format.GetCount()];

    for(uint i=0;i<atta_count;i++)
    {
        attachments[i].flags            = 0;
        attachments[i].samples          = VK_SAMPLE_COUNT_1_BIT;
        attachments[i].loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;      //LOAD_OP_CLEAR代表LOAD时清空内容
        attachments[i].storeOp          = VK_ATTACHMENT_STORE_OP_STORE;     //STORE_OP_STROE代表SOTRE时储存内容
        attachments[i].stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  //DONT CARE表示不在意
        attachments[i].stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[i].initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;        //代表不关心初始布局
    }

    const VkFormat *cf=color_format.GetData();
    for(int i=0;i<color_format.GetCount();i++)
    {
        attachments[i].finalLayout      = color_final_layout;
        attachments[i].format           = *cf;
        ++cf;
    }

    if(depth!=-1)
    {
        attachments[depth].finalLayout  = depth_final_layout;
        attachments[depth].format       = depth_format;
    }

    const uint32_t subpass_count=subpass.GetCount();
    uint32_t dependency_count;

    SharedArray<VkSubpassDependency> dependency;
    
    if(subpass_count==1)
    {
        dependency_count=1;
        dependency=new VkSubpassDependency[dependency_count];
        
        dependency[0].srcSubpass        = VK_SUBPASS_EXTERNAL;
        dependency[0].dstSubpass        = 0;
        dependency[0].srcStageMask      = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependency[0].dstStageMask      = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependency[0].srcAccessMask     = VK_ACCESS_MEMORY_READ_BIT;
        dependency[0].dstAccessMask     = VK_ACCESS_MEMORY_READ_BIT;
        dependency[0].dependencyFlags   = VK_DEPENDENCY_BY_REGION_BIT;
    }
    else
    {
        dependency_count=subpass_count+1;

        dependency=new VkSubpassDependency[dependency_count];

        for(uint32_t i=0;i<subpass_count+1;i++)
        {
            if(i==0)
            {
                dependency[i].srcSubpass        = VK_SUBPASS_EXTERNAL;
                dependency[i].dstSubpass        = 0;
                dependency[i].srcStageMask      = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;            
                dependency[i].dstStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependency[i].srcAccessMask     = VK_ACCESS_MEMORY_READ_BIT;
                dependency[i].dstAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            }
            else
            {
                dependency[i].srcSubpass        = i-1;
                dependency[i].srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

                if(i==subpass_count)
                {
                    dependency[i].dstSubpass        = VK_SUBPASS_EXTERNAL;
                    dependency[i].dstStageMask      = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                    dependency[i].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    dependency[i].dstAccessMask     = VK_ACCESS_MEMORY_READ_BIT;
                }
                else
                {
                    dependency[i].dstSubpass        = i;
                    dependency[i].dstStageMask      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    dependency[i].srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    dependency[i].dstAccessMask     = VK_ACCESS_SHADER_READ_BIT;
                }
            }

            dependency[i].dependencyFlags  = VK_DEPENDENCY_BY_REGION_BIT;
        }
    }

    VkRenderPassCreateInfo rp_info;
    rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext           = nullptr;
    rp_info.attachmentCount = atta_count;
    rp_info.pAttachments    = attachments;
    rp_info.subpassCount    = subpass.GetCount();
    rp_info.pSubpasses      = subpass.GetData();
    rp_info.dependencyCount = dependency_count;
    rp_info.pDependencies   = dependency;

    VkRenderPass render_pass;

    if(vkCreateRenderPass(attr->device,&rp_info,nullptr,&render_pass)!=VK_SUCCESS)
        return(nullptr);

    return(new RenderPass(attr->device,render_pass,color_format,depth_format));
}

RenderPass *Device::CreateRenderPass(VkFormat color_format,VkFormat depth_format,VkImageLayout color_final_layout,VkImageLayout depth_final_layout)
{
    VkAttachmentReference color_atta_ref={0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depth_atta_ref={1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    List<VkSubpassDescription> subpass_desc_list;

    VkSubpassDescription subpass;
        
    subpass.flags                   = 0;
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount    = 0;
    subpass.pInputAttachments       = nullptr;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &color_atta_ref;
    subpass.pResolveAttachments     = nullptr;
    subpass.pDepthStencilAttachment = &depth_atta_ref;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments    = nullptr;

    subpass_desc_list.Add(subpass);

    List<VkFormat> color_format_list;

    color_format_list.Add(color_format);

    return CreateRenderPass(subpass_desc_list,color_format_list,depth_format,color_final_layout,depth_final_layout);
}
VK_NAMESPACE_END
