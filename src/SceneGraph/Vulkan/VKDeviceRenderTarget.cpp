#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
RenderTarget *GPUDevice::CreateRenderTarget(Framebuffer *fb,const uint32_t fence_count)
{
    return(new RenderTarget(this,fb,fence_count));
}

RenderTarget *GPUDevice::CreateRenderTarget(const FramebufferInfo *fbi,const uint32_t fence_count)
{
    if(!fbi)return(nullptr);

    RenderPass *rp=CreateRenderPass(fbi);       //Renderpass内部会验证格式，所以不需要自己处理

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
        {
            delete rp;
            return(nullptr);
        }

        *tp++=color_texture;
        *iv++=color_texture->GetImageView();
    }

    Texture2D *depth_texture=(depth_format!=FMT_UNDEFINED)?CreateTexture2D(new DepthAttachmentTextureCreateInfo(depth_format,extent)):nullptr;

    Framebuffer *fb=CreateFramebuffer(rp,color_iv_list,color_count,depth_texture?depth_texture->GetImageView():nullptr);

    if(fb)
    {
        RenderTarget *rt=new RenderTarget(this,rp,fb,color_texture_list,color_count,depth_texture,fence_count);

        color_texture_list.DiscardObject();
        return rt;
    }

    SAFE_CLEAR(depth_texture);
    delete rp;
    return nullptr;
}
VK_NAMESPACE_END