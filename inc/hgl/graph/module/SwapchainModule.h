#pragma once

#include<hgl/graph/module/RenderModule.h>

VK_NAMESPACE_BEGIN

class SwapchainModule:public GraphModule
{
    Swapchain *swapchain=nullptr;

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
    
    //virtual void OnExecute(const double,RenderCmdBuffer *);

public:

    GRAPH_MODULE_CONSTRUCT(SwapchainModule)
    virtual ~SwapchainModule();

    bool Init() override;

    bool BeginFrame();
    void EndFrame();

public:

            RenderPass *    GetRenderPass   ()      {return swapchain_rp;}

            RTSwapchain *   GetRenderTarget ()      {return swapchain_rt;}

    const   VkExtent2D &    GetSwapchainSize()const {return swapchain_rt->GetExtent();}

};//class SwapchainModule:public GraphModule

VK_NAMESPACE_END
