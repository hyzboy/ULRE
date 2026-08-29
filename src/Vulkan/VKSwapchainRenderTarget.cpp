#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKSwapchain.h>
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
}

SwapchainRenderTarget::~SwapchainRenderTarget()
{
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
        LogWarning("vkAcquireNextImageKHR: OUT_OF_DATE");
        return false;
    }
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

bool SwapchainRenderTarget::Submit()
{
    SwapchainFrameSync& slot  = sync_slots[current_slot];
    SwapchainImage*     image = swapchain->sc_image + acquired_image;

    // Submit: wait image_available, signal render_finished
    if (!slot.queue->Submit(image->cmd_buf, slot.image_available, slot.render_finished))
    {
        LogError("SwapchainRenderTarget: queue submit failed (slot=%u image=%u)",
                 current_slot, acquired_image);
        current_slot = (current_slot + 1) % slot_count;
        return false;
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
        LogWarning("vkQueuePresentKHR: result=%d (OUT_OF_DATE or SUBOPTIMAL)", (int)result);
        return false;
    }
    if (result != VK_SUCCESS)
    {
        LogError("vkQueuePresentKHR failed, result=%d", (int)result);
        return false;
    }

    return true;
}

bool SwapchainRenderTarget::Submit(Semaphore* /*wait_sem*/)
{
    // Swapchain RT manages its own image_available semaphore; external wait_sem is ignored
    return Submit();
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

Semaphore* SwapchainRenderTarget::GetRenderCompleteSemaphore()
{
    return sync_slots[current_slot].render_finished;
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
