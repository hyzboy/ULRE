#include<hgl/graph/vulkan/VKDevice.h>

VK_NAMESPACE_BEGIN
RenderTarget *Device::CreateRenderTarget(Framebuffer *fb)
{
    CommandBuffer *cb=CreateCommandBuffer(fb->GetExtent(),fb->GetAttachmentCount());

    return(new RenderTarget(this,fb,cb));
}

RenderTarget *Device::CreateRenderTarget(   const uint w,const uint h,
                                            const List<VkFormat> &color_format_list,
                                            const VkFormat depth_format,
                                            const VkImageLayout color_layout,
                                            const VkImageLayout depth_layout)
{
    if(w<=0||h<=0)return(nullptr);

    RenderPass *rp=CreateRenderPass(color_format_list,depth_format,color_layout,depth_layout);       //Renderpass内部会验证格式，所以不需要自己处理

    ObjectList<Texture2D> color_texture_list;
    List<ImageView *> color_texture_image_view_list;

    for(const VkFormat &fmt:color_format_list)
    {
        Texture2D *color_texture=CreateAttachmentTextureColor(fmt,w,h);

        if(!color_texture)
        {
            delete rp;
            return(nullptr);
        }

        color_texture_list.Add(color_texture);
        color_texture_image_view_list.Add(color_texture->GetImageView());
    }

    Texture2D *depth_texture=(depth_format!=FMT_UNDEFINED)?CreateAttachmentTextureDepth(depth_format,w,h):nullptr;

    Framebuffer *fb=CreateFramebuffer(rp,color_texture_image_view_list,depth_texture->GetImageView());

    if(!fb)
    {
        SAFE_CLEAR(depth_texture)
        delete rp;
        return nullptr;
    }

    return(CreateRenderTarget(fb));
}

RenderTarget *Device::CreateRenderTarget(   const uint w,const uint h,
                                            const VkFormat color_format,
                                            const VkFormat depth_format,
                                            const VkImageLayout color_layout,
                                            const VkImageLayout depth_layout)
{
    if(w<=0||h<=0)return(nullptr);

    List<VkFormat> color_format_list;

    color_format_list.Add(color_format);

    return CreateRenderTarget(w,h,color_format_list,depth_format,color_layout,depth_layout);
}
VK_NAMESPACE_END