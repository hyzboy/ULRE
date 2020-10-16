#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKImageView.h>
#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/type/Smart.h>

VK_NAMESPACE_BEGIN

Framebuffer::Framebuffer(VkDevice dev,VkFramebuffer fb,VkFramebufferCreateInfo *fb_create_info,bool depth)
{
    device=dev;
    frame_buffer=fb;
    fb_info=fb_create_info;

    extent.width=fb_info->width;
    extent.height=fb_info->height;

    has_depth=depth;
    if(has_depth)
        color_count=fb_info->attachmentCount-1;
    else
        color_count=fb_info->attachmentCount;

    depth_texture=nullptr;
}

Framebuffer::~Framebuffer()
{
    SAFE_CLEAR(depth_texture);
    color_texture.Clear();

    vkDestroyFramebuffer(device,frame_buffer,nullptr);
}
VK_NAMESPACE_END
