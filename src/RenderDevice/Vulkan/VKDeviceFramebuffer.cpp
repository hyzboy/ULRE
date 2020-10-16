#include<hgl/graph/vulkan/VKDevice.h>

VK_NAMESPACE_BEGIN
Framebuffer *Device::CreateFramebuffer(RenderPass *rp,ImageView **color_list,const uint color_count,ImageView *depth)
{    
    uint att_count=color_count;

    if(depth)++att_count;
    
    VkImageView *attachments=new VkImageView[att_count];

    if(color_count)
    {
        const List<VkFormat> &cf_list=rp->GetColorFormat();

        const VkFormat *cf=cf_list.GetData();
        ImageView **iv=color_list;
        for(uint i=0;i<color_count;i++)
        {
            if(*cf!=(*iv)->GetFormat())
                return(nullptr);

            attachments[i]=**iv;

            ++cf;
            ++iv;
        }
    }

    if(depth)
    {
        if(rp->GetDepthFormat()!=depth->GetFormat())
        {
            delete[] attachments;
            return(nullptr);
        }

        attachments[color_count]=*depth;
    }

    const VkExtent3D extent=depth->GetExtent();

    FramebufferCreateInfo *fb_info=new FramebufferCreateInfo;

    fb_info->renderPass      = *rp;
    fb_info->attachmentCount = att_count;
    fb_info->pAttachments    = attachments;
    fb_info->width           = extent.width;
    fb_info->height          = extent.height;
    fb_info->layers          = 1;

    VkFramebuffer fb;

    if(vkCreateFramebuffer(GetDevice(),fb_info,nullptr,&fb)!=VK_SUCCESS)
        return(nullptr);

    return(new Framebuffer(GetDevice(),fb,fb_info,depth));
}

Framebuffer *Device::CreateFramebuffer(RenderPass *rp,List<ImageView *> &color,ImageView *depth)
{    
    if(!rp)return(nullptr);

    if(rp->GetColorFormat().GetCount()!=color.GetCount())return(nullptr);

    if(color.GetCount()==0&&!depth)return(nullptr);

    return CreateFramebuffer(rp,color.GetData(),color.GetCount(),depth);
}

Framebuffer *Device::CreateFramebuffer(RenderPass *rp,ImageView *color,ImageView *depth)
{
    if(!rp)return(nullptr);
    if(!color&&!depth)return(nullptr);

    return CreateFramebuffer(rp,&color,1,depth);
}

Framebuffer *Device::CreateFramebuffer(RenderPass *rp,ImageView *iv)
{
    if(!rp)return(nullptr);
    if(!iv)return(nullptr);

    if(iv->hasColor())
        return CreateFramebuffer(rp,&iv,1,nullptr);
    else
    if(iv->hasDepth())
        return CreateFramebuffer(rp,nullptr,0,iv);
    else
        return nullptr;
}
VK_NAMESPACE_END