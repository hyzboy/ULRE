﻿#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/type/Smart.h>

VK_NAMESPACE_BEGIN

Framebuffer::Framebuffer(VkDevice dev,VkFramebuffer fb,const VkExtent2D &ext,VkRenderPass rp,uint32_t cc,bool depth)
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
