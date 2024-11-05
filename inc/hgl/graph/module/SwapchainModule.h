#pragma once

#include<hgl/graph/module/RenderModule.h>

VK_NAMESPACE_BEGIN

class SwapchainModule:public GraphModule
{
    Swapchain *swapchain=nullptr;

    RTSwapchain *swapchain_rt=nullptr;

private:

    bool CreateSwapchainFBO(Swapchain *);

    Swapchain *CreateSwapchain(const VkExtent2D &acquire_extent);

public:

    GRAPH_MODULE_CONSTRUCT(Swapchain)
    virtual ~SwapchainModule()=default;

    bool Init() override;
};//class SwapchainModule:public RenderModule

VK_NAMESPACE_END
