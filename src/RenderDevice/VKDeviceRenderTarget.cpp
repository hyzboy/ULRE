#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN
RenderTarget *GPUDevice::CreateRenderTarget(Framebuffer *fb,const uint32_t fence_count)
{
    GPUCmdBuffer *cb=CreateCommandBuffer(fb->GetExtent(),fb->GetAttachmentCount());

    return(new RenderTarget(this,fb,cb,fence_count));
}

RenderTarget *GPUDevice::CreateRenderTarget(const uint w,const uint h,
                                            const List<VkFormat> &color_format_list,
                                            const VkFormat depth_format,
                                            const VkImageLayout color_layout,
                                            const VkImageLayout depth_layout,
                                            const uint32_t fence_count)
{
    if(w<=0||h<=0)return(nullptr);

    RenderPass *rp=CreateRenderPass(color_format_list,depth_format,color_layout,depth_layout);       //Renderpass内部会验证格式，所以不需要自己处理

    if(!rp)return(nullptr);

    const uint32_t color_count=color_format_list.GetCount();

    const VkExtent3D extent={w,h,1};

    AutoDeleteObjectArray<Texture2D> color_texture_list(color_count);
    AutoDeleteArray<ImageView *> color_iv_list(color_count);        //iv只是从Texture2D中取出来的，无需一个个delete

    Texture2D **tp=color_texture_list;
    ImageView **iv=color_iv_list;

    for(const VkFormat &fmt:color_format_list)
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
        GPUCmdBuffer *cb=CreateCommandBuffer(fb->GetExtent(),fb->GetAttachmentCount());

        if(cb)
        {
            RenderTarget *rt=new RenderTarget(this,rp,fb,cb,color_texture_list,color_count,depth_texture,fence_count);

            color_texture_list.DiscardObject();
            return rt;
        }
    }

    SAFE_CLEAR(depth_texture);
    delete rp;
    return nullptr;
}

RenderTarget *GPUDevice::CreateRenderTarget(const uint w,const uint h,
                                            const VkFormat color_format,
                                            const VkFormat depth_format,
                                            const VkImageLayout color_layout,
                                            const VkImageLayout depth_layout,
                                            const uint32_t fence_count)
{
    if(w<=0||h<=0)return(nullptr);

    List<VkFormat> color_format_list;

    color_format_list.Add(color_format);

    return CreateRenderTarget(w,h,color_format_list,depth_format,color_layout,depth_layout,fence_count);
}

RenderTarget *GPUDevice::CreateRenderTarget(const uint w,const uint h,const VkFormat format,const VkImageLayout final_layout,const uint32_t fence_count)
{
    if(w<=0||h<=0)return(nullptr);

    if(format==FMT_UNDEFINED)return(nullptr);

    List<VkFormat> color_format_list;

    if(IsDepthFormat(format))
    {
        return CreateRenderTarget(w,h,color_format_list,format,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,final_layout,fence_count);
    }
    else
    {
        color_format_list.Add(format);
    
        return CreateRenderTarget(w,h,color_format_list,FMT_UNDEFINED,final_layout,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,fence_count);
    }        
}
VK_NAMESPACE_END