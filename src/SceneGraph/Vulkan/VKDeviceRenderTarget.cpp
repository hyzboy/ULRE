#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
RenderTarget *GPUDevice::CreateRT(const FramebufferInfo *fbi,RenderPass *rp,const uint32_t fence_count)
{
    if(!fbi)return(nullptr);
    if(!rp)return(nullptr);

    const uint32_t color_count=fbi->GetColorCount();
    const VkExtent2D extent=fbi->GetExtent();
    const VkFormat depth_format=fbi->GetDepthFormat();

    AutoDeleteObjectArray<Texture2D> color_texture_list(color_count);
    AutoDeleteArray<ImageView *> color_iv_list(color_count);        //iv只是从Texture2D中取出来的，无需一个个delete

    Texture2D **tp=color_texture_list;
    ImageView **iv=color_iv_list;

    for(const VkFormat &fmt:fbi->GetColorFormatList())
    {
        Texture2D *color_texture=CreateTexture2D(new ColorAttachmentTextureCreateInfo(fmt,extent));

        if(!color_texture)
            return(nullptr);

        *tp++=color_texture;
        *iv++=color_texture->GetImageView();
    }

    Texture2D *depth_texture=(depth_format!=PF_UNDEFINED)?CreateTexture2D(new DepthAttachmentTextureCreateInfo(depth_format,extent)):nullptr;

    Framebuffer *fb=CreateFBO(rp,color_iv_list,color_count,depth_texture?depth_texture->GetImageView():nullptr);

    if(fb)
    {
        DeviceQueue *q=CreateQueue(fence_count,false);
        Semaphore *render_complete_semaphore=CreateGPUSemaphore();

        RenderTarget *rt=new RenderTarget(q,render_complete_semaphore,rp,fb,color_texture_list,color_count,depth_texture);

        color_texture_list.DiscardObject();
        return rt;
    }

    SAFE_CLEAR(depth_texture);
    return nullptr;
}

RenderTarget *GPUDevice::CreateRT(const FramebufferInfo *fbi,const uint32_t fence_count)
{
    if(!fbi)return(nullptr);

    RenderPass *rp=AcquireRenderPass(fbi);

    if(!rp)return(nullptr);

    return CreateRT(fbi,rp,fence_count);
}

RTSwapchain *GPUDevice::CreateSwapchainRenderTarget()
{
    Swapchain *sc=CreateSwapchain(attr->surface_caps.currentExtent);

    if(!sc)
        return(nullptr);

    DeviceQueue *q=CreateQueue(sc->image_count,false);
    Semaphore *render_complete_semaphore=CreateGPUSemaphore();
    Semaphore *present_complete_semaphore=CreateGPUSemaphore();

    RTSwapchain *srt=new RTSwapchain(   attr->device,
                                        sc,
                                        q,
                                        render_complete_semaphore,
                                        present_complete_semaphore,
                                        device_render_pass
                                        );

    return srt;
}
VK_NAMESPACE_END
