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

    // 主帧车道（A1）：timeline 信号量。主帧提交 signal 一个递增值；离屏 prepass
    // await 上一次主帧的值 ⇒ GPU 侧保证「prepass 覆写阴影前，在途主帧已读完」。
    if (VulkanDevice *device = GetDevice())
        main_lane = device->CreateTimelineSemaphore("Swapchain:MainLane");

    if (!main_lane)
        LogError("SwapchainRenderTarget: 主帧车道（timeline 信号量）创建失败");
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
        LogWarning("vkAcquireNextImageKHR: OUT_OF_DATE");
        return false;
    }
    if (result == VK_SUBOPTIMAL_KHR)
        resize_required = true;

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        LogError("vkAcquireNextImageKHR failed, result=%d", (int)result);
        return false;
    }

    // 3. If this swapchain image is still in-flight from a different slot, wait that slot too
    DeviceQueue* prev_owner = images_in_flight[acquired_image];
    if (prev_owner && prev_owner != slot.queue)
        prev_owner->WaitLastSubmitFence();

    // 4. Claim this image for the current slot
    images_in_flight[acquired_image] = slot.queue;

    return true;
}

bool SwapchainRenderTarget::Submit(const SemaphoreSubmit *extra_waits,const uint32_t extra_wait_count)
{
    SwapchainFrameSync& slot  = sync_slots[current_slot];
    SwapchainImage*     image = swapchain->sc_image + acquired_image;

    // 等待列表：本槽 image_available（COLOR_ATTACHMENT_OUTPUT）+ 本帧 Transfer 完成信号量
    // + 调用方额外等待（A1：本帧各离屏 RT 的车道值）
    constexpr uint32_t MAX_WAITS = 16;
    SemaphoreSubmit waits[MAX_WAITS];
    uint32_t wait_count = 0;

    waits[wait_count++] = SemaphoreSubmit::WaitImage(slot.image_available);

    TextureUploadQueue *upload_queue = nullptr;
    bool has_upload_waits = false;

    if (ecs_context)
    {
        if (auto *gc = ecs_context->GetGraphicsContext())
        {
            if (auto *tm = gc->GetTextureManager())
            {
                upload_queue = tm->GetUploadQueue();
                if (upload_queue)
                {
                    const auto &sems = upload_queue->GetPendingWaitSemaphores();
                    const Semaphore *const *sem_list = sems.GetData();

                    for (uint32_t i = 0; i < sems.GetCount() && wait_count < MAX_WAITS; i++)
                    {
                        if (!sem_list[i])
                            continue;

                        waits[wait_count++] = SemaphoreSubmit::Wait(const_cast<Semaphore *>(sem_list[i]));
                        has_upload_waits = true;
                    }
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
    if (main_lane)
    {
        main_lane_value = main_lane->NextValue();
        signals[signal_count++] = SemaphoreSubmit::Signal(main_lane, main_lane_value);
    }

    // Submit: wait image_available + upload semaphores + 调用方额外等待（本帧各离屏 RT 车道）；signal render_finished + 主帧车道
    if (!slot.queue->Submit(image->cmd_buf, waits, wait_count, signals, signal_count))
    {
        LogError("SwapchainRenderTarget: queue submit failed (slot=%u image=%u)",
                 current_slot, acquired_image);
        current_slot = (current_slot + 1) % slot_count;
        return false;
    }

    if (upload_queue && has_upload_waits)
    {
        upload_queue->ClearPendingWaitSemaphores();
    }

    // Present: wait render_finished
    VkSemaphore rs = *(slot.render_finished);
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores    = &rs;
    present_info.pImageIndices      = &acquired_image;

    VkResult result = slot.queue->Present(&present_info);

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
