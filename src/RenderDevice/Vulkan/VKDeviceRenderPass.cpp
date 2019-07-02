#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKRenderPass.h>

VK_NAMESPACE_BEGIN
RenderPass *Device::CreateRenderPass(List<VkFormat> color_format,VkFormat depth_format,VkImageLayout color_final_layout,VkImageLayout depth_final_layout)
{
    int depth=-1;
    uint atta_count=color_format.GetCount();

    if(attr->physical_device->CheckDepthFormat(depth_format))
    {
        depth=atta_count++;
    }

    SharedArray<VkAttachmentDescription> attachments=new VkAttachmentDescription[atta_count];
    SharedArray<VkAttachmentReference> color_references=new VkAttachmentReference[color_format.GetCount()];
    VkAttachmentReference depth_references;

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

        color_references[i].attachment  = i;
        color_references[i].layout      = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass;
    subpass.flags                   = 0;
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount    = 0;
    subpass.pInputAttachments       = nullptr;
    subpass.colorAttachmentCount    = color_format.GetCount();
    subpass.pColorAttachments       = color_references;
    subpass.pResolveAttachments     = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments    = nullptr;

    if(depth!=-1)
    {
        attachments[depth].finalLayout  = depth_final_layout;
        attachments[depth].format       = depth_format;

        depth_references.attachment     = depth;
        depth_references.layout         = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        subpass.pDepthStencilAttachment = &depth_references;
    }
    else
        subpass.pDepthStencilAttachment = nullptr;

    VkSubpassDependency dependency;
    dependency.srcSubpass       = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass       = 0;
    dependency.srcStageMask     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask    = 0;
    dependency.dstAccessMask    = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags  = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rp_info;
    rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext           = nullptr;
    rp_info.attachmentCount = atta_count;
    rp_info.pAttachments    = attachments;
    rp_info.subpassCount    = 1;
    rp_info.pSubpasses      = &subpass;
    rp_info.dependencyCount = 1;
    rp_info.pDependencies   = &dependency;

    VkRenderPass render_pass;

    if(vkCreateRenderPass(attr->device,&rp_info,nullptr,&render_pass)!=VK_SUCCESS)
        return(nullptr);

    return(new RenderPass(attr->device,render_pass,color_format,depth_format));
}

RenderPass *Device::CreateRenderPass(VkFormat color_format,VkFormat depth_format,VkImageLayout color_final_layout,VkImageLayout depth_final_layout)
{
    List<VkFormat> color_format_list;

    color_format_list.Add(color_format);

    return CreateRenderPass(color_format_list,depth_format,color_final_layout,depth_final_layout);
}
VK_NAMESPACE_END
