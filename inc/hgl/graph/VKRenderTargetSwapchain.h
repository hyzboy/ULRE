#pragma once

#include<hgl/graph/VKRenderTargetMultiFrame.h>

VK_NAMESPACE_BEGIN

/**
* 交换链专用渲染目标
*/
class SwapchainRenderTarget:public MultiFrameRenderTarget
{
    Swapchain *swapchain;
    PresentInfo present_info;

    Semaphore *present_complete_semaphore=nullptr;

private:

    SwapchainRenderTarget(RenderFramework *rf,Swapchain *sc,Semaphore *pcs,RenderTarget **rtl);

    friend class SwapchainModule;
    friend class RenderTargetManager;

public:

    ~SwapchainRenderTarget() override;

public:

    IRenderTarget *AcquireNextImage();             ///<获取下一帧的索引

    bool Submit()override;                         ///<提交当前帧的渲染，交推送到前台
};//class SwapchainRenderTarget:public MultiFrameRenderTarget

VK_NAMESPACE_END
