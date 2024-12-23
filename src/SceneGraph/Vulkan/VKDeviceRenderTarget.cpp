#include<hgl/graph/VKDevice.h>
#include<hgl/graph/manager/TextureManager.h>
#include<hgl/graph/manager/RenderPassManager.h>

VK_NAMESPACE_BEGIN
RenderTarget *TextureManager::CreateRT(const FramebufferInfo *fbi,RenderPass *rp,const uint32_t fence_count)
{
    if(!fbi)return(nullptr);
    if(!rp)return(nullptr);

    const uint32_t image_count=fbi->GetColorCount();
    const VkExtent2D extent=fbi->GetExtent();
    const VkFormat depth_format=fbi->GetDepthFormat();

    AutoDeleteObjectArray<Texture2D> color_texture_list(image_count);
    AutoDeleteArray<ImageView *> color_iv_list(image_count);        //iv只是从Texture2D中取出来的，无需一个个delete

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

    Framebuffer *fb=CreateFBO(rp,color_iv_list,image_count,depth_texture?depth_texture->GetImageView():nullptr);

    if(fb)
    {
        auto *dev=GetDevice();

        DeviceQueue *q=dev->CreateQueue(fence_count,false);
        Semaphore *render_complete_semaphore=dev->CreateGPUSemaphore();

        RenderTarget *rt=new RenderTarget(q,render_complete_semaphore,rp,fb,color_texture_list,image_count,depth_texture);

        color_texture_list.DiscardObject();
        return rt;
    }

    SAFE_CLEAR(depth_texture);
    return nullptr;
}

RenderTarget *TextureManager::CreateRT(const FramebufferInfo *fbi,const uint32_t fence_count)
{
    if(!fbi)return(nullptr);
    if(!rp_manager)return(nullptr);     //这个判断理论上不可能成立

    RenderPass *rp=rp_manager->AcquireRenderPass(fbi);

    if(!rp)return(nullptr);

    return CreateRT(fbi,rp,fence_count);
}

VK_NAMESPACE_END
