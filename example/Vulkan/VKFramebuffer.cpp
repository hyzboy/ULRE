#include"VKFramebuffer.h"
#include"VKDevice.h"
#include"VKRenderPass.h"
VK_NAMESPACE_BEGIN
Framebuffer::~Framebuffer()
{
    vkDestroyFramebuffer(device,frame_buffer,nullptr);
}

Framebuffer *CreateFramebuffer(Device *dev,RenderPass *rp,VkImageView color,VkImageView depth)
{
    if(!dev||!rp||!color||!depth)return(nullptr);

    const VkExtent2D extent=dev->GetExtent();
    VkImageView attachments[2];

    attachments[0]=color;
    attachments[1]=depth;

    VkFramebufferCreateInfo fb_info = {};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.pNext = nullptr;
    fb_info.renderPass = *rp;
    fb_info.attachmentCount = 2;
    fb_info.pAttachments = attachments;
    fb_info.width = extent.width;
    fb_info.height = extent.height;
    fb_info.layers = 1;

    VkFramebuffer fb;

    if(vkCreateFramebuffer(dev->GetDevice(), &fb_info, nullptr, &fb)!=VK_SUCCESS)
        return(nullptr);

    return(new Framebuffer(dev->GetDevice(),fb));
}
VK_NAMESPACE_END
