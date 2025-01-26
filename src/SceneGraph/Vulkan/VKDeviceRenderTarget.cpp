#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDeviceRenderPassManage.h>
#include<hgl/graph/VKSemaphore.h>

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
        RenderTargetData *rtd=new RenderTargetData{};

        rtd->fbo                        =fb;
        rtd->queue                      =CreateQueue(fence_count,false);
        rtd->render_complete_semaphore  =CreateGPUSemaphore();

        rtd->cmd_buf                    =CreateRenderCommandBuffer("");

        rtd->color_count                =color_count;
        rtd->color_textures             =hgl_new_copy<Texture2D *>(color_texture_list,color_count);
        rtd->depth_texture              =depth_texture;

        color_texture_list.DiscardObject();

        return(new RenderTarget(rtd));
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

namespace
{
    struct SwapchainRenderTargetData:public RenderTargetData
    {
        void Clear() override
        {
            delete render_complete_semaphore;
            delete queue;

            delete[] color_textures;
        }
    };//
}//namespace

SwapchainRenderTarget *GPUDevice::CreateSwapchainRenderTarget()
{
    Swapchain *sc=CreateSwapchain(attr->surface_caps.currentExtent);

    if(!sc)
        return(nullptr);

    RenderTarget **rt_list=new RenderTarget*[sc->image_count];

    SwapchainImage *sc_image=sc->sc_image;

    for(uint32_t i=0;i<sc->image_count;i++)
    {
        RenderTargetData *rtd=new SwapchainRenderTargetData{};

        rtd->fbo                        =sc_image->fbo;
        rtd->queue                      =CreateQueue(sc->image_count,false);
        rtd->render_complete_semaphore  =CreateGPUSemaphore();

        rtd->cmd_buf                    =sc_image->cmd_buf;

        rtd->color_count                =1;
        rtd->color_textures             =new Texture2D*[1];
        rtd->color_textures[0]          =sc_image->color;
        rtd->depth_texture              =sc_image->depth;

        rt_list[i]=new RenderTarget(rtd);

        ++sc_image;
    }

    SwapchainRenderTarget *srt=new SwapchainRenderTarget(   attr->device,
                                                            sc,
                                                            CreateGPUSemaphore(),
                                                            rt_list
                                                            );

    return srt;
}
VK_NAMESPACE_END
