#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
RenderTarget *GPUDevice::CreateRenderTarget(const FramebufferInfo *fbi,RenderPass *rp,const uint32_t fence_count)
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

    Framebuffer *fb=CreateFramebuffer(rp,color_iv_list,color_count,depth_texture?depth_texture->GetImageView():nullptr);

    if(fb)
    {
        GPUQueue *q=CreateQueue(fence_count,false);
        GPUSemaphore *render_complete_semaphore=CreateGPUSemaphore();

        RenderTarget *rt=new RenderTarget(q,render_complete_semaphore,rp,fb,color_texture_list,color_count,depth_texture);

        color_texture_list.DiscardObject();
        return rt;
    }

    SAFE_CLEAR(depth_texture);
    return nullptr;
}

RenderTarget *GPUDevice::CreateRenderTarget(const FramebufferInfo *fbi,const uint32_t fence_count)
{
    if(!fbi)return(nullptr);

    RenderPass *rp=AcquireRenderPass(fbi);

    if(!rp)return(nullptr);

    return CreateRenderTarget(fbi,rp,fence_count);
}

SwapchainRenderTarget *GPUDevice::CreateSwapchainRenderTarget()
{
    const uint32_t count=swapchain->GetImageCount();

    GPUQueue *q=CreateQueue(count,false);
    GPUSemaphore *render_complete_semaphore=CreateGPUSemaphore();
    GPUSemaphore *present_complete_semaphore=CreateGPUSemaphore();

    Texture2D **sc_color=swapchain->GetColorTextures();
    Texture2D *sc_depth=swapchain->GetDepthTexture();

    Framebuffer **render_frame=new Framebuffer *[count];

    for(uint32_t i=0;i<count;i++)
    {
        render_frame[i]=CreateFramebuffer(device_render_pass,(*sc_color)->GetImageView(),sc_depth->GetImageView());
        ++sc_color;
    }

    SwapchainRenderTarget *srt=new SwapchainRenderTarget(   attr->device,
                                                            swapchain,
                                                            q,
                                                            render_complete_semaphore,
                                                            present_complete_semaphore,
                                                            device_render_pass,
                                                            render_frame
                                                            );

    return srt;
}
VK_NAMESPACE_END