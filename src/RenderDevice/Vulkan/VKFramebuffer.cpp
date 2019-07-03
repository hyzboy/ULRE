#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKImageView.h>
#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/type/Smart.h>

VK_NAMESPACE_BEGIN
Framebuffer::~Framebuffer()
{
    vkDestroyFramebuffer(device,frame_buffer,nullptr);
}

Framebuffer *CreateFramebuffer(Device *dev,RenderPass *rp,ImageView **color_list,const uint color_count,ImageView *depth)
{    
    uint att_count=color_count;

    if(depth)++att_count;
    
    SharedArray<VkImageView> attachments=new VkImageView[att_count];

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
            return(nullptr);

        attachments[color_count]=*depth;
    }

    const VkExtent2D extent=dev->GetExtent();

    VkFramebufferCreateInfo fb_info;
    fb_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.pNext           = nullptr;
    fb_info.flags           = 0;
    fb_info.renderPass      = *rp;
    fb_info.attachmentCount = att_count;
    fb_info.pAttachments    = attachments;
    fb_info.width           = extent.width;
    fb_info.height          = extent.height;
    fb_info.layers          = 1;

    VkFramebuffer fb;

    if(vkCreateFramebuffer(dev->GetDevice(), &fb_info, nullptr, &fb)!=VK_SUCCESS)
        return(nullptr);

    return(new Framebuffer(dev->GetDevice(),fb));
}

Framebuffer *CreateFramebuffer(Device *dev,RenderPass *rp,List<ImageView *> &color,ImageView *depth)
{    
    if(!dev)return(nullptr);
    if(!rp)return(nullptr);

    if(rp->GetColorFormat().GetCount()!=color.GetCount())return(nullptr);

    if(color.GetCount()==0&&!depth)return(nullptr);

    return CreateFramebuffer(dev,rp,color.GetData(),color.GetCount(),depth);
}

Framebuffer *CreateFramebuffer(Device *dev,RenderPass *rp,List<ImageView *> image_view_list)
{
    const int count=image_view_list.GetCount();

    ImageView *last_iv=*(image_view_list.GetData()+count-1);

    if(last_iv->GetAspectFlags()&VK_IMAGE_ASPECT_DEPTH_BIT)
        return CreateFramebuffer(dev,rp,image_view_list.GetData(),count-1,last_iv);
    else
        return CreateFramebuffer(dev,rp,image_view_list.GetData(),count,nullptr);
}

Framebuffer *CreateFramebuffer(Device *dev,RenderPass *rp,ImageView *color,ImageView *depth)
{
    if(!dev)return(nullptr);
    if(!rp)return(nullptr);
    if(!color&&!depth)return(nullptr);

    return CreateFramebuffer(dev,rp,&color,1,depth);
}

Framebuffer *CreateFramebuffer(Device *dev,RenderPass *rp,ImageView *depth)
{
    if(!dev)return(nullptr);
    if(!rp)return(nullptr);
    if(!depth)return(nullptr);

    return CreateFramebuffer(dev,rp,nullptr,0,depth);
}
VK_NAMESPACE_END
