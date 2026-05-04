#pragma once

#include<hgl/vk/VKRenderTargetMultiFrame.h>
#include<hgl/log/Log.h>

namespace hgl::graph{

/**
* 交换链专用渲染目标
*/
class SwapchainRenderTarget:public MultiFrameRenderTarget
{
    OBJECT_LOGGER

    Swapchain *swapchain;
    PresentInfo present_info;

    Semaphore **image_acquired_semaphores=nullptr;
    Semaphore *current_image_acquired_semaphore=nullptr;
    uint32_t next_acquire_semaphore_index=0;

private:

    SwapchainRenderTarget(hgl::ecs::ECSContext *ctx,Swapchain *sc,RenderTargetData *rtdl);

    friend class SwapchainModule;
    friend class RenderTargetManager;

public:

    ~SwapchainRenderTarget() override;

public:

    bool NextFrame  ()override;                             ///<获取下一帧的索引

    bool Submit     ()override;                             ///<提交当前帧的渲染，交推送到前台

    // Swapchain frames currently own independent DeviceQueue wrappers that point to
    // the same Vulkan queue handle. Wait all frame queues to avoid reusing shared
    // frame-global buffers while another wrapper still has in-flight submits.
    bool WaitFence  ()override;
    bool WaitQueue  ()override;

    /**
     * Release swapchain-owned resources (Swapchain and image-acquired semaphores)
     * Must be called by SwapchainModule before destroying this object
     */
    void ReleaseSwapchainResources();
};//class SwapchainRenderTarget:public MultiFrameRenderTarget

}//namespace hgl::graph
