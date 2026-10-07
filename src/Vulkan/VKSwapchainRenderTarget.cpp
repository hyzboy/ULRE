#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKSwapchain.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/TextureUploadQueue.h>
#include<hgl/Macro.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

SwapchainRenderTarget::SwapchainRenderTarget(
    hgl::ecs::ECSContext* ctx,
    Swapchain* sc,
    SwapchainFrameSync* slots,
    uint32_t slot_cnt)
    : IRenderTarget(ctx, sc->extent)
    , swapchain(sc)
    , sync_slots(slots)
    , slot_count(slot_cnt)
    , current_slot(0)
    , acquired_image(0)
{
    present_info.waitSemaphoreCount = 0;
    present_info.pWaitSemaphores    = nullptr;
    present_info.swapchainCount     = 1;
    present_info.pResults           = nullptr;
    present_info.pSwapchains        = &(swapchain->swap_chain);

    // Zero-initialise per-image ownership table
    images_in_flight = new DeviceQueue*[swapchain->image_count]();

    // 主帧车道（A1）**不在此创建**：构造期 `ecs_context` 还没绑定 GPU 设备
    //（`ECSContext::Initialize(device, target)` 的入参就是本 RT ⇒ 本 RT 必须先存在）
    // ⇒ 这里 `GetDevice()` 必为 null，建了也是恒 null 且静默失效。
    // 改为首次真正需要时经 `EnsureMainLane()` 延迟创建（见下方定义）。
}

/// 主帧车道（A1）**延迟创建**：构造期设备未绑定（见构造里的说明）——首次真正
/// 需要时（主帧提交 signal / 离屏 prepass 询问 GetMainLane）才建。
/// 车道是**单调递增值**：等"最新值"即可覆盖所有更早的在途主帧（同队列提交序 ⇒
/// 值 N 完成蕴含 N-1… 已完成）⇒ 单个 timeline 车道足以保护跨帧共享的阴影纹理。
Semaphore *SwapchainRenderTarget::EnsureMainLane()
{
    if (main_lane)
        return main_lane;

    VulkanDevice *device = GetDevice();
    if (!device)
        return nullptr;             // 设备仍未就绪：按"无车道"降级（不刷屏、不报错）

    main_lane = device->CreateTimelineSemaphore("Swapchain:MainLane");

    if (!main_lane)
    {
        LogError("SwapchainRenderTarget: 主帧车道（timeline 信号量）创建失败");
        return nullptr;
    }

    GLogInfo("[SwapchainRenderTarget] 主帧车道（A1 timeline 信号量）就位（延迟创建）");
    return main_lane;
}

SwapchainRenderTarget::~SwapchainRenderTarget()
{
    SAFE_CLEAR(main_lane);

    delete[] images_in_flight;
    images_in_flight = nullptr;

    if (sync_slots)
    {
        for (uint32_t i = 0; i < slot_count; i++)
        {
            SAFE_CLEAR(sync_slots[i].image_available);
            SAFE_CLEAR(sync_slots[i].render_finished);
            SAFE_CLEAR(sync_slots[i].queue);
        }
        delete[] sync_slots;
        sync_slots = nullptr;
    }

    SAFE_CLEAR(swapchain);
}

bool SwapchainRenderTarget::NextFrame()
{
    SwapchainFrameSync& slot = sync_slots[current_slot];

    // 1. Wait this slot's fence so its image_available semaphore is free to reuse
    slot.queue->WaitLastSubmitFence();

    // 2. Acquire next swapchain image, signalling this slot's image_available semaphore
    VkResult result = vkAcquireNextImageKHR(
        GetVkDevice(),
        swapchain->swap_chain,
        UINT64_MAX,
        *(slot.image_available),
        VK_NULL_HANDLE,
        &acquired_image);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        resize_required = true;
        acquire_window_open = false;
        LogWarning("vkAcquireNextImageKHR: OUT_OF_DATE");
        return false;
    }
    if (result == VK_SUBOPTIMAL_KHR)
        resize_required = true;

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        acquire_window_open = false;
        LogError("vkAcquireNextImageKHR failed, result=%d", (int)result);
        return false;
    }

    // 3. If this swapchain image is still in-flight from a different slot, wait that slot too
    DeviceQueue* prev_owner = images_in_flight[acquired_image];
    if (prev_owner && prev_owner != slot.queue)
        prev_owner->WaitLastSubmitFence();

    // 4. Claim this image for the current slot
    images_in_flight[acquired_image] = slot.queue;

    // 5. 颜色读回窗口开启：本帧这张 image 已是「已 acquire、未 present」
    //    （Submit 收尾时关闭；见 SwapchainRenderTarget::SetInFrameReadbackHook）
    acquire_window_open = true;

    return true;
}

bool SwapchainRenderTarget::Submit(const SemaphoreSubmit *extra_waits,const uint32_t extra_wait_count)
{
    SwapchainFrameSync& slot  = sync_slots[current_slot];
    SwapchainImage*     image = swapchain->sc_image + acquired_image;

    // 等待列表：本槽 image_available（COLOR_ATTACHMENT_OUTPUT）+ 上传车道（latest signal）
    // + 调用方额外等待（A1：本帧各离屏 RT 的车道值）
    constexpr uint32_t MAX_WAITS = 16;
    SemaphoreSubmit waits[MAX_WAITS];
    uint32_t wait_count = 0;

    waits[wait_count++] = SemaphoreSubmit::WaitImage(slot.image_available);

    if (ecs_context)
    {
        if (auto *gc = ecs_context->GetGraphicsContext())
        {
            if (auto *tm = gc->GetTextureManager())
            {
                if (auto *uq = tm->GetUploadQueue())
                {
                    // 上传车道 timeline：**单个等待项覆盖全部在飞 DMA 拷贝**
                    //（旧逐任务二进制 sem 收集：溢出丢弃 = 裸用未同步纹理；且提交后未消费即被
                    //  任务析构删除 = VUID-vkDestroySemaphore-semaphore-05149）
                    Semaphore *lane = uq->GetUploadLane();
                    const uint64_t lane_value = uq->GetUploadLaneValue();
                    if (lane && lane_value != 0 && wait_count < MAX_WAITS)
                        waits[wait_count++] = SemaphoreSubmit::Wait(lane, lane_value);
                }
            }
        }
    }

    for (uint32_t i = 0; i < extra_wait_count && wait_count < MAX_WAITS; i++)
        if (extra_waits[i].semaphore)
            waits[wait_count++] = extra_waits[i];

    SemaphoreSubmit extra_signal = SemaphoreSubmit::Signal(slot.render_finished);
    SemaphoreSubmit signals[2];
    uint32_t signal_count = 0;

    signals[signal_count++] = extra_signal;

    // 主帧车道（A1）：signal 一个递增值，供下一次离屏 prepass await
    if (Semaphore *lane = EnsureMainLane())
    {
        main_lane_value = lane->NextValue();
        signals[signal_count++] = SemaphoreSubmit::Signal(lane, main_lane_value);
    }

    // Submit: wait image_available + upload lane + 调用方额外等待（本帧各离屏 RT 车道）；signal render_finished + 主帧车道
    if (!slot.queue->Submit(image->cmd_buf, waits, wait_count, signals, signal_count))
    {
        LogError("SwapchainRenderTarget: queue submit failed (slot=%u image=%u)",
                 current_slot, acquired_image);
        current_slot = (current_slot + 1) % slot_count;
        acquire_window_open = false;
        return false;
    }

    // ── 帧内回读窗口：本帧已提交、尚未 present ───────────────────────────────
    // 此刻交换链 image 仍是「已 acquire、未 present」，是唯一能对它做布局转换/拷贝的时点。
    // 钩子内（如 ReadbackColorTarget）会排空图形队列，故拷贝拿到的是本帧已完成的画面；
    // 随后 present 照常在 render_finished 上等待，与本窗口无冲突。
    if (in_frame_readback_hook)
    {
        auto hook = std::move(in_frame_readback_hook);
        in_frame_readback_hook = nullptr;      // 一次性：执行后即清除

        hook();
    }

    // Present: wait render_finished
    VkSemaphore rs = *(slot.render_finished);
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &rs;
    present_info.pImageIndices      = &acquired_image;

    VkResult result = slot.queue->Present(&present_info);

    // present 之后该 image 已归还交换链 ⇒ 本帧颜色访问窗口关闭（所有路径一致）
    acquire_window_open = false;

    // Advance slot regardless of present result
    current_slot = (current_slot + 1) % slot_count;

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        resize_required = true;
        LogWarning("vkQueuePresentKHR: result=%d (OUT_OF_DATE or SUBOPTIMAL)", (int)result);
        return result == VK_SUBOPTIMAL_KHR;
    }
    if (result != VK_SUCCESS)
    {
        LogError("vkQueuePresentKHR failed, result=%d", (int)result);
        return false;
    }

    return true;
}

bool SwapchainRenderTarget::WaitFence()
{
    // Full drain: wait all slots (use before resize / recreation / descriptor updates)
    bool ok = true;
    for (uint32_t i = 0; i < slot_count; i++)
        if (!sync_slots[i].queue->WaitLastSubmitFence())
            ok = false;
    return ok;
}

bool SwapchainRenderTarget::WaitQueue()
{
    return sync_slots[current_slot].queue->WaitQueue();
}

Framebuffer* SwapchainRenderTarget::GetFramebuffer()
{
    return swapchain->sc_image[acquired_image].fbo;
}

RenderPass* SwapchainRenderTarget::GetRenderPass()
{
    return swapchain->sc_image[acquired_image].fbo->GetRenderPass();
}

uint32_t SwapchainRenderTarget::GetColorCount()
{
    return 1;   // swapchain always has exactly 1 colour attachment
}

bool SwapchainRenderTarget::hasDepth()
{
    return swapchain->depth_format != VK_FORMAT_UNDEFINED;
}

Texture2D* SwapchainRenderTarget::GetColorTexture(int index)
{
    if (index != 0) return nullptr;
    return swapchain->sc_image[acquired_image].color;
}

Texture2D* SwapchainRenderTarget::GetDepthTexture()
{
    return swapchain->sc_image[acquired_image].depth;
}

DeviceQueue* SwapchainRenderTarget::GetQueue()
{
    return sync_slots[current_slot].queue;
}

RenderCmdBuffer* SwapchainRenderTarget::GetRenderCmdBuffer()
{
    return swapchain->sc_image[acquired_image].cmd_buf;
}

RenderCmdBuffer* SwapchainRenderTarget::BeginRender()
{
    RenderCmdBuffer* cb = swapchain->sc_image[acquired_image].cmd_buf;
    if (!cb) return nullptr;
    cb->Begin();
    // Dynamic Rendering：不再 BindFramebuffer——附件由 RenderSystemCore::BeginRenderPass
    // 经 IRenderTarget::GetColorAttachment/GetDepthAttachment 提供（见 VKCommandBuffer.h）
    return cb;
}

void SwapchainRenderTarget::EndRender()
{
    RenderCmdBuffer* cb = swapchain->sc_image[acquired_image].cmd_buf;
    if (cb) cb->End();
}

}//namespace hgl::graph
