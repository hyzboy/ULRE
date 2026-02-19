#include <cstdint>
#include<hgl/ecs/core/Context.h>
#include <hgl/graph/GraphTypes.h>
#include <hgl/graph/module/GraphModule.h>
#include <hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/TextureManager.h>
#include <hgl/Macro.h>
#include <hgl/type/MemoryAlloc.h>
#include <hgl/type/Smart.h>
#include<hgl/vk/VKDevice.h>
#include <hgl/vk/VKFormat.h>
#include <hgl/vk/VKFramebuffer.h>
#include <hgl/vk/VKImageView.h>
#include <hgl/vk/VKNamespace.h>
#include <hgl/vk/VKRenderbufferInfo.h>
#include <hgl/vk/VKRenderPass.h>
#include <hgl/vk/VKRenderTargetData.h>
#include<hgl/vk/VKRenderTargetSingle.h>
#include <hgl/vk/VKTexture.h>
#include <hgl/vk/VKTextureCreateInfo.h>
#include <vulkan/vulkan_core.h>

namespace hgl::graph{

RenderTargetManager::RenderTargetManager(GraphicsContext *gc,hgl::ecs::ECSContext *ecs_ctx,TextureManager *tm,RenderPassManager *rpm)
    :GraphModuleInherit<RenderTargetManager,GraphModule>(gc,"RenderTargetManager")
{
    tex_manager=tm;
    rp_manager=rpm;
    ecs_context=ecs_ctx;
}

RenderTarget *RenderTargetManager::CreateRT(const AnsiString &name, const FramebufferInfo *fbi,RenderPass *rp,const uint32_t fence_count)
{
    if(!fbi)return(nullptr);
    if(!rp)return(nullptr);
    if(!ecs_context)return(nullptr);

    const uint32_t color_count=fbi->GetColorCount();
    const VkExtent2D extent=fbi->GetExtent();
    const VkFormat depth_format=fbi->GetDepthFormat();

    AutoDeleteObjectArray<Texture2D> color_texture_list(color_count);
    AutoDeleteArray<ImageView *> color_iv_list(color_count);        //iv只是从Texture2D中取出来的，无需一个个delete

    Texture2D **tp=color_texture_list;
    ImageView **iv=color_iv_list;

    uint32_t color_index=0;
    for(const VkFormat &fmt:fbi->GetColorFormatList())
    {
        U8String tex_name = ToU8String(name + ":Color[" + AnsiString::numberOf(color_index) + "]");
        Texture2D *color_texture=tex_manager->CreateTexture2D(new ColorAttachmentTextureCreateInfo(fmt,extent,tex_name));

        if(!color_texture)
            return(nullptr);

        *tp++=color_texture;
        *iv++=color_texture->GetImageView();
        color_index++;
    }

    U8String depth_name = ToU8String(name + ":Depth");
    Texture2D *depth_texture=(depth_format!=PF_UNDEFINED)?tex_manager->CreateTexture2D(new DepthAttachmentTextureCreateInfo(depth_format,extent,depth_name)):nullptr;

    Framebuffer *fb=CreateFBO(rp,color_iv_list,color_count,depth_texture?depth_texture->GetImageView():nullptr);

    if(fb)
    {
        RenderTargetData *rtd=new RenderTargetData{};

        VulkanDevice *dev=GetDevice();

        ObjectNameBuilder rt_name = ObjectNameBuilder(name).Append(ObjectTypeTag::RenderTarget);
        rtd->fbo=fb;
        rtd->queue=dev->CreateQueue(rt_name, fence_count, false);
        rtd->render_complete_semaphore=dev->CreateGPUSemaphore(rt_name);
        rtd->cmd_buf=dev->CreateRenderCommandBuffer(rt_name);

        rtd->color_count=color_count;
        rtd->color_textures=new_copy<Texture2D *>(color_texture_list,color_count);
        rtd->depth_texture=depth_texture;

        color_texture_list.Discard();

        return(new RenderTarget(ecs_context,rtd));
    }

    SAFE_CLEAR(depth_texture);
    return nullptr;
}

RenderTarget *RenderTargetManager::CreateRT(const AnsiString &name, const FramebufferInfo *fbi,const uint32_t fence_count)
{
    if(!fbi)return(nullptr);

    RenderPass *rp=rp_manager->AcquireRenderPass(fbi);

    if(!rp)return(nullptr);

    return CreateRT(name, fbi,rp,fence_count);
}

RenderTarget *RenderTargetManager::CreateRTFromGraphicsContext(GraphicsContext *gc, hgl::ecs::ECSContext *ecs_ctx,
                                                               const FramebufferInfo *fbi, const uint32_t fence_count)
{
    // Generate a default name from the extent
    if(!fbi)
        return(nullptr);

    const VkExtent2D extent = fbi->GetExtent();
    const AnsiString auto_name = "RT_" + AnsiString::numberOf(extent.width) + "x" + AnsiString::numberOf(extent.height);

    return CreateRTFromGraphicsContext(gc, ecs_ctx, auto_name, fbi, fence_count);
}

RenderTarget *RenderTargetManager::CreateRTFromGraphicsContext(GraphicsContext *gc, hgl::ecs::ECSContext *ecs_ctx,
                                                               const AnsiString &name, const FramebufferInfo *fbi, const uint32_t fence_count)
{
    if(!gc || !ecs_ctx || !fbi)
        return(nullptr);

    VulkanDevice *device = gc->GetDevice();
    TextureManager *tex_manager = gc->GetTextureManager();
    RenderPassManager *rp_manager = gc->GetRenderPassManager();

    if(!device || !tex_manager || !rp_manager)
        return(nullptr);

    RenderPass *rp = rp_manager->AcquireRenderPass(fbi);
    if(!rp)
        return(nullptr);

    const uint32_t color_count = fbi->GetColorCount();
    const VkExtent2D extent = fbi->GetExtent();
    const VkFormat depth_format = fbi->GetDepthFormat();

    AutoDeleteObjectArray<Texture2D> color_texture_list(color_count);
    AutoDeleteArray<ImageView *> color_iv_list(color_count);

    Texture2D **tp = color_texture_list;
    ImageView **iv = color_iv_list;

    uint32_t color_index = 0;
    for(const VkFormat &fmt : fbi->GetColorFormatList())
    {
        U8String tex_name = ToU8String(name + ":Color[" + AnsiString::numberOf(color_index) + "]");
        Texture2D *color_texture = tex_manager->CreateTexture2D(new ColorAttachmentTextureCreateInfo(fmt, extent, tex_name));
        if(!color_texture)
            return(nullptr);

        *tp++ = color_texture;
        *iv++ = color_texture->GetImageView();
        color_index++;
    }

    U8String depth_name = ToU8String(name + ":Depth");
    Texture2D *depth_texture = (depth_format != PF_UNDEFINED) ? tex_manager->CreateTexture2D(new DepthAttachmentTextureCreateInfo(depth_format, extent, depth_name)) : nullptr;

    // Create framebuffer (RenderTargetManager is a friend of Framebuffer, so we can instantiate directly here).
    auto create_vk_framebuffer = [](VkDevice vk_device, RenderPass *render_pass, const VkExtent2D &fb_extent,
                                    VkImageView *attachments, const uint attachment_count) -> VkFramebuffer
    {
        FramebufferCreateInfo fb_info;

        fb_info.renderPass = render_pass->GetVkRenderPass();
        fb_info.attachmentCount = attachment_count;
        fb_info.pAttachments = attachments;
        fb_info.width = fb_extent.width;
        fb_info.height = fb_extent.height;
        fb_info.layers = 1;

        VkFramebuffer fb = VK_NULL_HANDLE;
        if(vkCreateFramebuffer(vk_device, &fb_info, nullptr, &fb) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        return fb;
    };

    auto create_fbo = [&](RenderPass *render_pass, ImageView **color_list, const uint color_count, ImageView *depth) -> Framebuffer *
    {
        if(!render_pass)
            return nullptr;

        uint att_count = color_count;
        if(depth)
            ++att_count;

        AutoDeleteArray<VkImageView> attachments(att_count);
        VkImageView *ap = attachments;

        if(color_count)
        {
            const ValueArray<VkFormat> &cf_list = render_pass->GetColorFormat();
            const VkFormat *cf = cf_list.GetData();
            ImageView **iv_list = color_list;

            for(uint i = 0; i < color_count; ++i)
            {
                if(*cf != (*iv_list)->GetFormat())
                    return nullptr;

                *ap = (*iv_list)->GetImageView();
                ++ap;
                ++cf;
                ++iv_list;
            }
        }

        VkExtent2D fb_extent;

        if(depth)
        {
            if(render_pass->GetDepthFormat() != depth->GetFormat())
                return nullptr;

            attachments[color_count] = depth->GetImageView();
            fb_extent.width = depth->GetExtent().width;
            fb_extent.height = depth->GetExtent().height;
        }
        else
        {
            fb_extent.width = color_list[0]->GetExtent().width;
            fb_extent.height = color_list[0]->GetExtent().height;
        }

        VkFramebuffer fb = create_vk_framebuffer(device->GetDevice(), render_pass, fb_extent, attachments, att_count);
        if(!fb)
            return nullptr;
        {
            AnsiString name = "Framebuffer_" + AnsiString::numberOf((uint64_t)(uintptr_t)fb);
            device->TrackObject(VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)(uintptr_t)fb, ObjectNameBuilder(name).Append(ObjectTypeTag::VKFramebuffer));
        }
        return new Framebuffer(device->GetDevice(), fb, fb_extent, render_pass, color_count, depth != nullptr);
    };

    Framebuffer *fb = create_fbo(rp, color_iv_list, color_count, depth_texture ? depth_texture->GetImageView() : nullptr);

    if(fb)
    {
        RenderTargetData *rtd = new RenderTargetData{};

        const AnsiString rt_name = name + ":RT";
        rtd->fbo = fb;
        rtd->queue = device->CreateQueue(rt_name, fence_count, false);
        rtd->render_complete_semaphore = device->CreateGPUSemaphore(rt_name);
        rtd->cmd_buf = device->CreateRenderCommandBuffer(rt_name);

        rtd->color_count = color_count;
        rtd->color_textures = new_copy<Texture2D *>(color_texture_list, color_count);
        rtd->depth_texture = depth_texture;

        color_texture_list.Discard();

        return new RenderTarget(ecs_ctx, rtd);
    }

    SAFE_CLEAR(depth_texture);
    return nullptr;
}

}//namespace hgl::graph
