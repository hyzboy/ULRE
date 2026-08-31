#pragma once

#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKSwapchain.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

/**
 * Per-slot (in-flight frame) synchronization objects.
 * slot_count == swapchain image_count (configurable later).
 * Each slot owns its own imageAvailable / renderFinished semaphores and a queue with 1 fence.
 */
struct SwapchainFrameSync
{
    Semaphore*   image_available  = nullptr;  ///< vkAcquireNextImageKHR signals this
    Semaphore*   render_finished  = nullptr;  ///< vkQueueSubmit signals, vkQueuePresentKHR waits
    DeviceQueue* queue            = nullptr;  ///< Owns 1 fence; WaitLastSubmitFence = slot is free
};

/**
 * Swapchain render target with standard Vulkan WSI synchronization model:
 *
 *   NextFrame()  : wait slot fence -> acquire (signals image_available) -> image ownership check
 *   Submit()     : submit (wait image_available, signal render_finished) -> present (wait render_finished) -> advance slot
 *   WaitFence()  : full drain of all slots (used before resize / recreation)
 *
 * Frame-slot index (current_slot) and acquired swapchain image index (acquired_image) are
 * tracked independently so out-of-order image returns are handled correctly.
 */
class SwapchainRenderTarget : public IRenderTarget
{
    OBJECT_LOGGER

    Swapchain*          swapchain           = nullptr;
    PresentInfo         present_info;

    uint32_t            slot_count          = 0;        ///< Number of in-flight frame slots
    uint32_t            current_slot        = 0;        ///< Which slot is being processed this frame
    uint32_t            acquired_image      = 0;        ///< Image index from vkAcquireNextImageKHR

    SwapchainFrameSync* sync_slots          = nullptr;  ///< [slot_count] - owned
    DeviceQueue**       images_in_flight    = nullptr;  ///< [image_count] - which slot owns each image

    friend class SwapchainModule;

    SwapchainRenderTarget(hgl::ecs::ECSContext* ctx,
                          Swapchain* sc,
                          SwapchainFrameSync* slots,
                          uint32_t slot_cnt);

public:

    ~SwapchainRenderTarget() override;

    /// Acquire the next swapchain image. Handles slot-fence waiting and image ownership tracking.
    bool NextFrame();

    // --- IRenderTarget interface ---
    Framebuffer*        GetFramebuffer()                    override;
    RenderPass*         GetRenderPass()                     override;
    uint32_t            GetColorCount()                     override;
    bool                hasDepth()                          override;
    Texture2D*          GetColorTexture(int index = 0)      override;
    Texture2D*          GetDepthTexture()                   override;

    DeviceQueue*        GetQueue()                          override;
    Semaphore*          GetRenderCompleteSemaphore()         override;
    RenderCmdBuffer*    GetRenderCmdBuffer()                 override;

    bool                Submit(Semaphore* wait_sem)          override;
    bool                Submit()                             override;

    bool                WaitQueue()                          override;
    bool                WaitFence()                          override;

    RenderCmdBuffer*    BeginRender()                        override;
    void                EndRender()                          override;

    uint32_t            GetCurrentFrameIndex()  const       override { return acquired_image; }
    uint32_t            GetFrameCount()         const       override { return slot_count; }

};//class SwapchainRenderTarget

}//namespace hgl::graph
