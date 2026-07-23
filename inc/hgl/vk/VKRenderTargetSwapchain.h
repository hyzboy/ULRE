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

    Semaphore *present_complete_semaphore=nullptr;

private:

    SwapchainRenderTarget(hgl::ecs::ECSContext *ctx,Swapchain *sc,Semaphore *pcs,RenderTargetData *rtdl);

    friend class SwapchainModule;
    friend class RenderTargetManager;

public:

    ~SwapchainRenderTarget() override;

public:

    bool NextFrame  ()override;                             ///<获取下一帧的索引

    bool Submit     ()override;                             ///<提交当前帧的渲染，交推送到前台

    /**
     * Swapchain paths may share descriptor sets across multiple in-flight frames.
     * We must wait all frame queues before descriptor updates to avoid
     * vkUpdateDescriptorSets-on-pending-command-buffer validation errors.
     */
    bool WaitFence  ()override;

    /**
     * Release swapchain-owned resources (Swapchain and present_complete_semaphore)
     * Must be called by SwapchainModule before destroying this object
     */
    void ReleaseSwapchainResources();
};//class SwapchainRenderTarget:public MultiFrameRenderTarget

}//namespace hgl::graph
