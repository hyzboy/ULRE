#include<hgl/graph/VKDevice.h>
#include<hgl/graph/manager/TextureManager.h>

VK_NAMESPACE_BEGIN
VkFramebuffer CreateVulkanFramebuffer(VkDevice device,RenderPass *rp,const VkExtent2D &extent,VkImageView *attachments,const uint attachmentCount)
{
    FramebufferCreateInfo fb_info;

    fb_info.renderPass      = rp->GetVkRenderPass();
    fb_info.attachmentCount = attachmentCount;
    fb_info.pAttachments    = attachments;
    fb_info.width           = extent.width;
    fb_info.height          = extent.height;
    fb_info.layers          = 1;

    VkFramebuffer fb;

    if(vkCreateFramebuffer(device,&fb_info,nullptr,&fb)!=VK_SUCCESS)
        return(nullptr);

    return fb;
}

Framebuffer *TextureManager::CreateFBO(RenderPass *rp,ImageView **color_list,const uint image_count,ImageView *depth)
{
    uint att_count=image_count;

    if(depth)++att_count;
    
    AutoDeleteArray<VkImageView> attachments(att_count);
    VkImageView *ap=attachments;

    if(image_count)
    {
        const List<VkFormat> &cf_list=rp->GetColorFormat();

        const VkFormat *cf=cf_list.GetData();
        ImageView **iv=color_list;

        for(uint i=0;i<image_count;i++)
        {
            if(*cf!=(*iv)->GetFormat())
                return(nullptr);

            *ap=(*iv)->GetImageView();

            ++ap;
            ++cf;
            ++iv;
        }
    }

    VkExtent2D extent;

    if(depth)
    {
        if(rp->GetDepthFormat()!=depth->GetFormat())
        {
            delete[] attachments;
            return(nullptr);
        }

        attachments[image_count]=depth->GetImageView();

        extent.width=depth->GetExtent().width;
        extent.height=depth->GetExtent().height;
    }
    else
    {
        extent.width=color_list[0]->GetExtent().width;
        extent.height=color_list[0]->GetExtent().height;
    }

    VkFramebuffer fbo=CreateVulkanFramebuffer(GetVkDevice(),rp,extent,attachments,att_count);

    if(!fbo)
        return(nullptr);

    return(new Framebuffer(GetVkDevice(),fbo,extent,rp->GetVkRenderPass(),image_count,depth));
}
//
//Framebuffer *GPUDevice::CreateFBO(RenderPass *rp,List<ImageView *> &color,ImageView *depth)
//{    
//    if(!rp)return(nullptr);
//
//    if(rp->GetColorFormat().GetCount()!=color.GetCount())return(nullptr);
//
//    if(color.GetCount()==0&&!depth)return(nullptr);
//
//    return CreateFBO(rp,color.GetData(),color.GetCount(),depth);
//}

Framebuffer *TextureManager::CreateFBO(RenderPass *rp,ImageView *color,ImageView *depth)
{
    if(!rp)return(nullptr);
    if(!color&&!depth)return(nullptr);

    return CreateFBO(rp,&color,1,depth);
}

Framebuffer *TextureManager::CreateFBO(RenderPass *rp,ImageView *iv)
{
    if(!rp)return(nullptr);
    if(!iv)return(nullptr);

    if(iv->hasColor())
        return CreateFBO(rp,&iv,1,nullptr);
    else
    if(iv->hasDepth())
        return CreateFBO(rp,nullptr,0,iv);
    else
        return nullptr;
}
VK_NAMESPACE_END