#include<hgl/graph/vulkan/VKDevice.h>

VK_NAMESPACE_BEGIN
RenderTarget *Device::CreateRenderTarget(Framebuffer *fb)
{
    CommandBuffer *cb=CreateCommandBuffer(fb->GetExtent(),fb->GetAttachmentCount());

    return(new RenderTarget(this,fb,cb));
}

RenderTarget *Device::CreateRenderTarget(   const uint w,const uint h,
                                            const VkFormat color_format,
                                            const VkFormat depth_format,
                                            const VkImageLayout color_layout,
                                            const VkImageLayout depth_layout)
{
    if(w<=0||h<=0)return(nullptr);

    RenderPass *rp=CreateRenderPass(color_format,depth_format,color_layout,depth_layout);       //Renderpass内部会验证格式，所以不需要自己处理

    if(!CheckTextureFormatSupport(color_format))return(nullptr);
    if(!CheckTextureFormatSupport(depth_format))return(nullptr);

    Texture2D *color_texture=CreateAttachmentTextureColor(color_format,w,h);
    Texture2D *depth_texture=CreateAttachmentTextureDepth(depth_format,w,h);

    Framebuffer *fb=CreateFramebuffer(rp,color_texture->GetImageView(),depth_texture->GetImageView());

    return(CreateRenderTarget(fb));
}

//RenderTarget *Device::CreateRenderTarget(const uint w,const uint h,const List<VkFormat> &fmt_list)
//{
//    if(w<=0||h<=0||fmt_list.GetCount()<=0)return(nullptr);
//
//    uint color_count=0;
//    uint depth_count=0;     //只能有一个
//    uint stencil_count=0;
//
//    for(VkFormat fmt:fmt_list)
//    {
//        if(IsDepthFormat(fmt))++depth_count;
//        else
//        if(IsStencilFormat(fmt))++stencil_count;
//        else
//            ++color_count;
//
//        if(CheckTextureFormatSupport(fmt))
//            return(nullptr);
//    }
//
//    if(depth_count>1)return(nullptr);
//    if(stencil_count>1)return(nullptr);
//    
//    List<VkFormat> color_format_list;
//    VkFormat depth_format;
//    List<VkAttachmentDescription> desc_list;
//    List<VkAttachmentReference> color_ref_list;
//    VkAttachmentReference depth_ref;
//    List<vulkan::ImageView *> image_view_list;
//
//    for(VkFormat fmt:fmt_list)
//    {
//        Texture2D *tex=nullptr;
//        
//        if(IsDepthFormat(fmt))
//        {
//            tex=CreateAttachmentTextureDepth(fmt,w,h);
//
//            depth_format=fmt;
//        }
//        else
//        {
//            tex=CreateAttachmentTextureColor(fmt,w,h);
//
//            image_view_list.Add(tex->GetImageView());
//            color_format_list.Add(fmt);
//        }
//    }
//
//    if(depth_count>0)CreateDepthAttachmentReference(&depth_ref,color_count);
//    if(color_count>0)CreateColorAttachmentReference(color_ref_list,0,color_count);
//
//    CreateAttachmentDescription(desc_list,color_format_list,depth_format,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
//
//    VkSubpassDescription sd;
//
//    CreateSubpassDescription(sd,color_ref_list,&depth_ref);
//}
VK_NAMESPACE_END