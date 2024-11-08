#pragma once

#include<hgl/graph/module/RenderModule.h>

VK_NAMESPACE_BEGIN

class SwapchainModule:public GraphModule
{
    Swapchain *swapchain=nullptr;

    RTSwapchain *swapchain_rt=nullptr;

protected:

    bool CreateSwapchain();

    bool CreateSwapchainRenderTarget();

    bool CreateSwapchainFBO(Swapchain *);


public:

    GRAPH_MODULE_CONSTRUCT(SwapchainModule)
    virtual ~SwapchainModule()=default;

    bool Init() override;

            RTSwapchain *   GetRenderTarget ()      {return swapchain_rt;}

    const   VkExtent2D &    GetSwapchainSize()const {return swapchain_rt->GetExtent();}
};//class SwapchainModule:public GraphModule

VK_NAMESPACE_END
