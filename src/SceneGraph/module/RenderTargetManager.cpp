#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/VKRenderTargetSingle.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/RenderPassManager.h>

VK_NAMESPACE_BEGIN

RenderTargetManager::RenderTargetManager(RenderFramework *rf,TextureManager *tm,RenderPassManager *rpm):GraphModuleInherit<RenderTargetManager,GraphModule>(rf,"RenderTargetManager")
{
    tex_manager=tm;
    rp_manager=rpm;
}

RenderTarget *RenderTargetManager::CreateRT(const FramebufferInfo *fbi,RenderPass *rp,const uint32_t fence_count)
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
        Texture2D *color_texture=tex_manager->CreateTexture2D(new ColorAttachmentTextureCreateInfo(fmt,extent));

        if(!color_texture)
            return(nullptr);

        *tp++=color_texture;
        *iv++=color_texture->GetImageView();
    }

    Texture2D *depth_texture=(depth_format!=PF_UNDEFINED)?tex_manager->CreateTexture2D(new DepthAttachmentTextureCreateInfo(depth_format,extent)):nullptr;

    Framebuffer *fb=CreateFBO(rp,color_iv_list,color_count,depth_texture?depth_texture->GetImageView():nullptr);

    if(fb)
    {
        RenderTargetData *rtd=new RenderTargetData{};

        GPUDevice *dev=GetDevice();

        rtd->fbo                        =fb;
        rtd->queue                      =dev->CreateQueue(fence_count,false);
        rtd->render_complete_semaphore  =dev->CreateGPUSemaphore();

        rtd->cmd_buf                    =dev->CreateRenderCommandBuffer("");

        rtd->color_count                =color_count;
        rtd->color_textures             =hgl_new_copy<Texture2D *>(color_texture_list,color_count);
        rtd->depth_texture              =depth_texture;

        color_texture_list.DiscardObject();

        return(new RenderTarget(GetRenderFramework(),rtd));
    }

    SAFE_CLEAR(depth_texture);
    return nullptr;
}

RenderTarget *RenderTargetManager::CreateRT(const FramebufferInfo *fbi,const uint32_t fence_count)
{
    if(!fbi)return(nullptr);

    RenderPass *rp=rp_manager->AcquireRenderPass(fbi);

    if(!rp)return(nullptr);

    return CreateRT(fbi,rp,fence_count);
}

VK_NAMESPACE_END
