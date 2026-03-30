#include <cstdint>
#include <vulkan/vulkan.h>
#include<hgl/ecs/core/Context.h>
#include <hgl/graph/GraphTypes.h>
#include <hgl/graph/module/GraphModule.h>
#include <hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/module/TextureManager.h>
#include <hgl/Macro.h>
#include <hgl/type/MemoryAlloc.h>
#include <hgl/type/Smart.h>
#include<hgl/vk/VKDevice.h>
#include <hgl/vk/VKFormat.h>
#include <hgl/vk/VKImageView.h>
#include <hgl/vk/VKRenderbufferInfo.h>
#include <hgl/vk/VKRenderFormat.h>
#include <hgl/vk/VKRenderTargetData.h>
#include<hgl/vk/VKRenderTargetSingle.h>
#include <hgl/vk/VKTexture.h>
#include <hgl/vk/VKTextureCreateInfo.h>
#include <vulkan/vulkan_core.h>

namespace hgl::graph{

RenderTargetManager::RenderTargetManager(GraphicsContext *gc,hgl::ecs::ECSContext *ecs_ctx,TextureManager *tm)
    :GraphModuleInherit<RenderTargetManager,GraphModule>(gc,"RenderTargetManager")
{
    tex_manager=tm;
    ecs_context=ecs_ctx;
}

RenderTarget *RenderTargetManager::CreateRT(const AnsiString &name, const FramebufferInfo *fbi,RenderFormat *rf,const uint32_t fence_count)
{
    if(!fbi)return(nullptr);
    if(!rf)return(nullptr);
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

    {
        RenderTargetData *rtd=new RenderTargetData{};

        VulkanDevice *dev=GetDevice();

        ObjectNameBuilder rt_name = ObjectNameBuilder(name).Append(ObjectTypeTag::RenderTarget);
        rtd->render_format=rf;
        rtd->extent=extent;
        rtd->queue=dev->CreateQueue(rt_name, fence_count, false);
        rtd->render_complete_semaphore=dev->CreateGPUSemaphore(rt_name);
        rtd->cmd_buf=dev->CreateRenderCommandBuffer(rt_name);

        rtd->color_count=color_count;
        rtd->color_textures=new_copy<Texture2D *>(color_texture_list,color_count);
        rtd->depth_texture=depth_texture;

        color_texture_list.Discard();

        return(new RenderTarget(ecs_context,rtd));
    }
}

RenderTarget *RenderTargetManager::CreateRT(const AnsiString &name, const FramebufferInfo *fbi,const uint32_t fence_count)
{
    if(!fbi)return(nullptr);

    RenderFormat *rf=GetDevice()->AcquireRenderFormat(fbi);

    if(!rf)return(nullptr);

    return CreateRT(name, fbi,rf,fence_count);
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

    if(!device || !tex_manager)
        return(nullptr);

    RenderFormat *rf = device->AcquireRenderFormat(fbi);
    if(!rf)
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

    // Create framebuffer (dynamic rendering: not needed, just track rp+extent).
    {
        RenderTargetData *rtd = new RenderTargetData{};

        const AnsiString rt_name = name + ":RT";
        rtd->render_format = rf;
        rtd->extent = extent;
        rtd->queue = device->CreateQueue(rt_name, fence_count, false);
        rtd->render_complete_semaphore = device->CreateGPUSemaphore(rt_name);
        rtd->cmd_buf = device->CreateRenderCommandBuffer(rt_name);

        rtd->color_count = color_count;
        rtd->color_textures = new_copy<Texture2D *>(color_texture_list, color_count);
        rtd->depth_texture = depth_texture;

        color_texture_list.Discard();

        return new RenderTarget(ecs_ctx, rtd);
    }
}
}//namespace hgl::graph
