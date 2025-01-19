#pragma once

#include<hgl/graph/module/RenderModule.h>

VK_NAMESPACE_BEGIN

class RenderTargetManager;

RENDER_MODULE_CLASS(SwapchainModule)
{
    Swapchain *swapchain=nullptr;

    TextureManager *tex_manager=nullptr;
    RenderTargetManager *rt_manager=nullptr;
    RenderPassManager *rp_manager=nullptr;

    RTSwapchain *swapchain_rt=nullptr;

    RenderPass *swapchain_rp=nullptr;

    RenderCmdBuffer **cmd_buf=nullptr;

protected:

    bool CreateSwapchainFBO();
    bool CreateSwapchain();
    bool CreateSwapchainRenderTarget();

    void InitRenderCmdBuffer();
    void ClearRenderCmdBuffer();

public:

    virtual void OnResize(const VkExtent2D &)override;                                              ///<窗口大小改变

public:

    SwapchainModule(GPUDevice *,TextureManager *tm,RenderTargetManager *rtm,RenderPassManager *rpm);
    virtual ~SwapchainModule();

    bool BeginFrame();
    void EndFrame();

public:

            RenderPass *    GetRenderPass   ()      {return swapchain_rp;}

            RTSwapchain *   GetRenderTarget ()      {return swapchain_rt;}

    const   VkExtent2D &    GetSwapchainSize()const {return swapchain_rt->GetExtent();}

            RenderCmdBuffer *GetRenderCmdBuffer();

};//class SwapchainModule:public RenderModule

VK_NAMESPACE_END
