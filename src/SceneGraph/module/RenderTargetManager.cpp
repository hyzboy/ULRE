#include <cstdint>
#include <vulkan/vulkan.h>
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

RenderTargetManager::~RenderTargetManager()
{
    Release();
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

    RenderTargetManager *rtm = gc->GetRenderTargetManager();
    if(!rtm)
        return(nullptr);

    return rtm->CreateOffscreenRT(ecs_ctx, name, fbi, fence_count);
}

RenderTarget *RenderTargetManager::CreateOffscreenRT(hgl::ecs::ECSContext *ecs_ctx,
                                                     const AnsiString &name,
                                                     const FramebufferInfo *fbi,
                                                     const uint32_t fence_count)
{
    if(!fbi || !ecs_ctx)
        return(nullptr);

    VulkanDevice *device = GetDevice();
    if(!device)
        return(nullptr);

    // 成员可能由构造函数传入；为空时回退到 GraphicsContext 上取（防御 rt_manager 先于其他模块构造）
    if(!tex_manager)
        tex_manager = GetGraphicsContext() ? GetGraphicsContext()->GetTextureManager() : nullptr;
    if(!rp_manager)
        rp_manager = GetGraphicsContext() ? GetGraphicsContext()->GetRenderPassManager() : nullptr;

    if(!tex_manager || !rp_manager)
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

    // 复用成员 CreateFBO（原先此处内联了一份逐行重复的 lambda 实现）
    Framebuffer *fb = CreateFBO(rp, color_iv_list, color_count, depth_texture ? depth_texture->GetImageView() : nullptr);

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

        RenderTarget *rt = new RenderTarget(ecs_ctx, rtd);

        // 登记入注册表：所有权归本 Manager，调用方用 Destroy() 而非 delete
        RenderTargetEntry entry;
        entry.name = name;
        entry.rt   = rt;
        registry.push_back(entry);

        return rt;
    }

    SAFE_CLEAR(depth_texture);
    return nullptr;
}

bool RenderTargetManager::Destroy(RenderTarget *rt)
{
    if(!rt)
        return(false);

    for(auto it = registry.begin(); it != registry.end(); ++it)
    {
        if(it->rt != rt)
            continue;

        registry.erase(it);
        delete rt;
        return(true);
    }

    return(false);
}

RenderTarget *RenderTargetManager::Find(const AnsiString &name)const
{
    for(const RenderTargetEntry &entry : registry)
    {
        if(entry.name == name)
            return entry.rt;
    }

    return(nullptr);
}

void RenderTargetManager::Release()
{
    // 统一回收所有仍登记的 RT（GraphModuleManager 销毁前调用）
    for(RenderTargetEntry &entry : registry)
    {
        if(entry.rt)
            delete entry.rt;
    }

    registry.clear();
}

void RenderTargetManager::OnResize(const VkExtent2D &extent)
{
    // 阶段 A：占位。阶段 D 起按 RenderTargetDesc 重建已标记 resizable 的 RT。
    (void)extent;
}

}//namespace hgl::graph
