#include<hgl/vk/VKFramebuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKImageView.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/type/Smart.h>

VK_NAMESPACE_BEGIN

Framebuffer::Framebuffer(VkDevice dev,VkFramebuffer fb,const VkExtent2D &ext,RenderPass *rp,uint32_t cc,bool depth)
{
    device=dev;
    frame_buffer=fb;
    render_pass=rp;

    extent=ext;
    color_count=cc;
    has_depth=depth;

    attachment_count=color_count;

    if(has_depth)
        ++attachment_count;
}

Framebuffer::~Framebuffer()
{
    vkDestroyFramebuffer(device,frame_buffer,nullptr);
}
VK_NAMESPACE_END
