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
#include<hgl/vk/VKOffscreenRenderTarget.h>
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

    // 离屏 RT 的 per-frame 数据槽带从主帧槽之上开始分配（见 RenderOptions.h）
    next_offscreen_slot_base = HGL_FRAME_SLOT_MAIN;

    // 回设到 GraphicsContext：其余 manager 由 module_manager->GetOrCreate 创建并
    // 赋给 GraphicsContext 成员，而 RTM 构造需要 ECSContext，只能外部创建。
    // 若此处不回设，gc->GetRenderTargetManager() 恒为 nullptr。
    if(gc)
        gc->SetRenderTargetManager(this);
}

RenderTargetManager::~RenderTargetManager()
{
    Release();
}

OffscreenRenderTarget *RenderTargetManager::CreateRTFromGraphicsContext(GraphicsContext *gc, hgl::ecs::ECSContext *ecs_ctx,
                                                                        const FramebufferInfo *fbi, const uint32_t slot_count)
{
    // Generate a default name from the extent
    if(!fbi)
        return(nullptr);

    const VkExtent2D extent = fbi->GetExtent();
    const AnsiString auto_name = "RT_" + AnsiString::numberOf(extent.width) + "x" + AnsiString::numberOf(extent.height);

    return CreateRTFromGraphicsContext(gc, ecs_ctx, auto_name, fbi, slot_count);
}

OffscreenRenderTarget *RenderTargetManager::CreateRTFromGraphicsContext(GraphicsContext *gc, hgl::ecs::ECSContext *ecs_ctx,
                                                                        const AnsiString &name, const FramebufferInfo *fbi, const uint32_t slot_count)
{
    if(!gc || !ecs_ctx || !fbi)
        return(nullptr);

    RenderTargetManager *rtm = gc->GetRenderTargetManager();
    if(!rtm)
        return(nullptr);

    return rtm->CreateOffscreenRT(ecs_ctx, name, fbi, slot_count);
}

void RenderTargetDeleter::operator()(IRenderTarget *rt)const
{
    if(!rt)
        return;

    if(manager)
        manager->Destroy(rt);
    else
        delete rt;      // Manager 不可达时的兜底
}

bool RenderTargetManager::ResolveFramebufferInfo(const RenderTargetDesc &desc,FramebufferInfo &fbi)
{
    auto *dev_attr = GetDevAttr();
    if(!dev_attr || !dev_attr->physical_device)
        return(false);

    // 颜色格式：desc 显式给出的优先；has_color 为真且未给出时补设备默认 surface format。
    // has_color 为假时保持零颜色附件——depth-only（如 shadow map）不能有颜色附件，
    // 若在此无条件补默认格式，"仅深度"将无法表达。
    std::vector<VkFormat> color_formats = desc.color_formats;

    if(desc.has_color && color_formats.empty())
        color_formats.push_back(dev_attr->surface_format.format);

    for(const VkFormat fmt : color_formats)
    {
        if(!fbi.AddColor(fmt))
            return(false);
    }

    if(desc.has_depth)
    {
        // 深度格式：desc 未指定时取设备默认深度格式
        VkFormat depth_format = desc.depth_format;

        if(depth_format == PF_UNDEFINED)
            depth_format = dev_attr->physical_device->GetDepthFormat();

        if(!fbi.SetDepth(depth_format))
            return(false);
    }

    fbi.SetExtent(desc.width, desc.height);
    return(true);
}

RenderTargetHandle RenderTargetManager::Create(const RenderTargetDesc &desc)
{
    RenderTargetHandle empty_handle;

    if(!desc.IsValid())
        return(empty_handle);

    // Swapchain RT 由 SwapchainModule 负责，Manager 只创建离屏 RT
    if(desc.kind != RenderTargetKind::Offscreen)
        return(empty_handle);

    if(!ecs_context)
        return(empty_handle);

    FramebufferInfo fbi;

    if(!ResolveFramebufferInfo(desc, fbi))
        return(empty_handle);

    AnsiString name = desc.name;

    if(name.IsEmpty())
        name = "RT_" + AnsiString::numberOf(desc.width) + "x" + AnsiString::numberOf(desc.height);

    OffscreenRenderTarget *rt = CreateOffscreenRT(ecs_context, name, &fbi, desc.slot_count);

    if(!rt)
        return(empty_handle);

    // 渲染参数收归 RT（阶段 C 起由 ECSContext::RenderTo 统一从 RT 读取）
    rt->SetClearColor(desc.clear_color);
    rt->SetEnvironmentProfile(desc.env_profile);

    // 登记 desc，供 Resize() 按 desc 重建
    for(RenderTargetEntry &entry : registry)
    {
        if(entry.rt == rt)
        {
            entry.desc = desc;
            break;
        }
    }

    RenderTargetDeleter deleter;
    deleter.manager = this;

    return RenderTargetHandle(rt, deleter);
}

OffscreenRenderTarget *RenderTargetManager::CreateOffscreenRT(hgl::ecs::ECSContext *ecs_ctx,
                                                              const AnsiString &name,
                                                              const FramebufferInfo *fbi,
                                                              const uint32_t slot_count)
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

    RenderTargetData *rtd = new RenderTargetData{};

    // ---- in-flight 槽（A7）----
    // 每个槽独占一组 {cmd_buf, queue(1 fence), render_complete_semaphore}，按提交次数轮转
    // （复用前等该槽 fence，见 RenderTargetData::BeginRender）；同时占一段 per-frame 数据槽带
    // （主帧槽之上整带分配，见 RenderOptions.h 的槽划分）。
    //
    // 固定上限、不为极端场景预留增长：带越界即 fail-fast（属项目 bug，不是要扩容的场景）。
    const uint32_t slots = (slot_count > 0) ? slot_count : 1;

    if(next_offscreen_slot_base + slots > HGL_FRAME_SLOT_TOTAL)
    {
        GLogError("[RenderTargetManager] %s: in-flight 槽带越界（起点=%u 槽数=%u 上限=%u）"
                 "—— 请减少该 RT 的 slot_count 或提高 HGL_FRAME_SLOT_TOTAL",
                 name.c_str(), next_offscreen_slot_base, slots, HGL_FRAME_SLOT_TOTAL);
        delete rtd;
        return(nullptr);
    }

    rtd->slot_count       = slots;
    rtd->slot_index       = slots - 1;              // 首次 BeginRender 前进后落在槽 0
    rtd->data_slot_base   = next_offscreen_slot_base;
    rtd->frame_slot_total = HGL_FRAME_SLOT_TOTAL;

    rtd->cmd_bufs                   = new RenderCmdBuffer *[slots]();
    rtd->queues                     = new DeviceQueue *[slots]();

    if(!rtd->cmd_bufs || !rtd->queues)
    {
        GLogError("[RenderTargetManager] %s: 槽数组分配失败", name.c_str());
        rtd->Clear();
        delete rtd;
        return(nullptr);
    }

    // 车道（A1）：本 RT 的 timeline 信号量 —— 提交时 signal、主帧提交 await 本帧的值，
    // 取代改造前的二进制「渲染完成」信号量（二进制每帧重复 signal 需要配对的等待方）。
    rtd->lane = device->CreateTimelineSemaphore(name + ":Lane");

    if(!rtd->lane)
    {
        GLogError("[RenderTargetManager] %s: 车道（timeline 信号量）创建失败", name.c_str());
        rtd->Clear();
        delete rtd;
        return(nullptr);
    }

    for(uint32_t i = 0; i < slots; i++)
    {
        const AnsiString slot_name = name + ":RT[slot" + AnsiString::numberOf(i) + "]";

        rtd->queues[i]   = device->CreateQueue(slot_name, 1, false);
        rtd->cmd_bufs[i] = device->CreateRenderCommandBuffer(slot_name);

        if(!rtd->queues[i] || !rtd->cmd_bufs[i])
        {
            GLogError("[RenderTargetManager] %s: 槽 %u 设备资源创建失败", name.c_str(), i);
            rtd->Clear();
            delete rtd;
            return(nullptr);
        }
    }

    next_offscreen_slot_base += slots;

    if(!CreateAttachments(rtd, name, fbi))
    {
        rtd->Clear();
        delete rtd;
        return(nullptr);
    }

    {
        const AnsiString rt_name = name + ":RT";

        OffscreenRenderTarget *rt = new OffscreenRenderTarget(ecs_ctx, rtd);

        // 登记入注册表：所有权归本 Manager，推荐用 RenderTargetHandle（RAII），
        // 裸指针场景用 Destroy()，不得手动 delete
        RenderTargetEntry entry;
        entry.name = name;
        entry.rt   = rt;
        registry.push_back(entry);

        return rt;
    }
}

bool RenderTargetManager::CreateAttachments(RenderTargetData *data,const AnsiString &name,const FramebufferInfo *fbi)
{
    if(!data || !fbi)
        return(false);

    if(!tex_manager || !rp_manager)
        return(false);

    const uint32_t   color_count  = fbi->GetColorCount();
    const VkExtent2D extent       = fbi->GetExtent();
    const VkFormat   depth_format = fbi->GetDepthFormat();

    // depth-only RT 的 color_count 为 0，此时深度是唯一附件。
    // 两者都为空意味着空附件，desc 校验已挡；此处再挡一次，
    // 避免落到 CreateFBO 中"无 depth 时读 color_list[0]"的解引用路径。
    if(color_count == 0 && depth_format == PF_UNDEFINED)
        return(false);

    RenderPass *rp = rp_manager->AcquireRenderPass(fbi);
    if(!rp)
        return(false);

    // 只存指针、不持所有权：纹理归 TextureManager，失败路径必须走 Release() 而不是
    // delete（原实现用 AutoDeleteObjectArray 会在失败路径 delete 纹理，
    // 在 TextureManager 侧留下悬挂指针）。
    // 长度至少 1：color_count 为 0 时循环不写这些数组，但要保证传给 CreateFBO
    // 的附件数组参数不是空指针。
    const uint32_t slot_count = (color_count > 0) ? color_count : 1;

    AutoDeleteArray<Texture2D *> color_texture_list(slot_count);
    AutoDeleteArray<ImageView *> color_iv_list(slot_count);

    Texture2D **tp = color_texture_list;
    ImageView **iv = color_iv_list;

    uint32_t created_color_count = 0;

    for(const VkFormat &fmt : fbi->GetColorFormatList())
    {
        U8String tex_name = ToU8String(name + ":Color[" + AnsiString::numberOf(created_color_count) + "]");
        Texture2D *color_texture = tex_manager->CreateTexture2D(new ColorAttachmentTextureCreateInfo(fmt, extent, tex_name));

        if(!color_texture)
        {
            for(uint32_t i = 0; i < created_color_count; ++i)
                tex_manager->Release(color_texture_list[i]);

            return(false);
        }

        *tp++ = color_texture;
        *iv++ = color_texture->GetImageView();
        created_color_count++;
    }

    Texture2D *depth_texture = nullptr;

    if(depth_format != PF_UNDEFINED)
    {
        U8String depth_name = ToU8String(name + ":Depth");
        depth_texture = tex_manager->CreateTexture2D(new DepthAttachmentTextureCreateInfo(depth_format, extent, depth_name));

        if(!depth_texture)
        {
            for(uint32_t i = 0; i < created_color_count; ++i)
                tex_manager->Release(color_texture_list[i]);

            return(false);
        }
    }

    // 复用成员 CreateFBO（原先此处内联了一份逐行重复的 lambda 实现）
    Framebuffer *fb = CreateFBO(rp, color_iv_list, color_count, depth_texture ? depth_texture->GetImageView() : nullptr);

    if(!fb)
    {
        for(uint32_t i = 0; i < created_color_count; ++i)
            tex_manager->Release(color_texture_list[i]);

        if(depth_texture)
            tex_manager->Release(depth_texture);

        return(false);
    }

    data->fbo            = fb;
    data->color_count    = color_count;
    data->color_textures = new_copy<Texture2D *>(color_texture_list.data(), color_count);
    data->depth_texture  = depth_texture;

    return(true);
}

bool RenderTargetManager::RebuildOffscreenRT(OffscreenRenderTarget *rt,const RenderTargetDesc &desc)
{
    if(!rt)
        return(false);

    RenderTargetData *data = rt->data;      // friend class RenderTargetManager
    if(!data)
        return(false);

    // ---- 1. 先解析出新配置 ----
    // 必须在释放旧资源之前完成：解析失败（如设备不可用）时旧纹理与 FBO 保持完好，
    // 否则 RT 会停在"资源已释放但没重建"的残废状态。
    FramebufferInfo fbi;

    if(!ResolveFramebufferInfo(desc, fbi))
        return(false);

    // ---- 2. 释放旧纹理（纹理由 TextureManager 创建，需显式 Release）----
    for(uint32_t i = 0; i < data->color_count; ++i)
    {
        if(data->color_textures[i])
            tex_manager->Release(data->color_textures[i]);
    }

    if(data->depth_texture)
        tex_manager->Release(data->depth_texture);

    // ---- 3. 销毁旧 FBO ----
    // 注意：queue / cmd_buf / render_complete_semaphore 由设备侧持有，
    // RenderTargetData::Clear() 只清指针不释放；此处若重新创建会造成设备对象累积，
    // 因此保留复用，仅重建纹理与 FBO。
    SAFE_CLEAR(data->fbo);

    delete[] data->color_textures;
    data->color_textures = nullptr;
    data->color_count    = 0;
    data->depth_texture  = nullptr;

    // ---- 4. 按新尺寸与附件配置重建 ----
    AnsiString name = desc.name;
    if(name.IsEmpty())
        name = "RT_" + AnsiString::numberOf(desc.width) + "x" + AnsiString::numberOf(desc.height);

    return CreateAttachments(data, name, &fbi);
}

bool RenderTargetManager::Resize(IRenderTarget *rt,const uint32_t width,const uint32_t height)
{
    if(!rt || width == 0 || height == 0)
        return(false);

    for(RenderTargetEntry &entry : registry)
    {
        if(entry.rt != rt)
            continue;

        if(!entry.desc.resizable)
            return(false);

        RenderTargetDesc desc = entry.desc;
        desc.width  = width;
        desc.height = height;

        if(!RebuildOffscreenRT(static_cast<OffscreenRenderTarget *>(rt), desc))
            return(false);

        entry.desc = desc;

        // 同步 RT 自身 extent 与视口 UBO
        VkExtent2D ext;
        ext.width  = width;
        ext.height = height;
        rt->OnResize(ext);

        return(true);
    }

    return(false);
}

bool RenderTargetManager::Destroy(IRenderTarget *rt)
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

IRenderTarget *RenderTargetManager::Find(const AnsiString &name)const
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
    // 只重建显式声明 follow_window 的 RT。
    // 离屏 RT 尺寸通常与窗口无关（如固定 512x512 的 RTT），不应被窗口缩放连带改变；
    // Swapchain RT 不在本 registry 中，由 SwapchainModule 自行处理。
    if(extent.width == 0 || extent.height == 0)
        return;

    for(RenderTargetEntry &entry : registry)
    {
        if(!entry.rt)
            continue;

        if(!entry.desc.follow_window || !entry.desc.resizable)
            continue;

        RenderTargetDesc desc = entry.desc;

        desc.width  = extent.width;
        desc.height = extent.height;

        if(!RebuildOffscreenRT(static_cast<OffscreenRenderTarget *>(entry.rt), desc))
        {
            GLogError("[RenderTargetManager] follow-window RT resize to %ux%u failed",
                      extent.width, extent.height);
            continue;
        }

        entry.desc = desc;
        entry.rt->OnResize(extent);
    }
}

}//namespace hgl::graph
