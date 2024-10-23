#pragma once

#include<hgl/graph/module/RenderModule.h>

VK_NAMESPACE_BEGIN

class SwapchainModule:public GraphModule
{
public:

    GRAPH_MODULE_CONSTRUCT(Swapchain)
    virtual ~SwapchainModule()=default;

};//class SwapchainModule:public RenderModule

VK_NAMESPACE_END
