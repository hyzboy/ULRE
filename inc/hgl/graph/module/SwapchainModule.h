#pragma once

#include<hgl/graph/module/GraphModule.h>

VK_NAMESPACE_BEGIN

class RenderTargetManager;
class RenderPassManager;

GRAPH_MODULE_CLASS(SwapchainModule)
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

    const   VkExtent2D &    GetSwapchainSize()const {return swapchain_rt->GetExtent();}

            RenderCmdBuffer *Use();

};//class SwapchainModule:public GraphModule

VK_NAMESPACE_END
