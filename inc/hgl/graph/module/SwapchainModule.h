#pragma once

#include<hgl/graph/module/RenderModule.h>

VK_NAMESPACE_BEGIN

class SwapchainModule:public GraphModule
{
public:

    static const char *GetModuleName(){return "Swapchain";} 

public:

    NO_COPY_NO_MOVE(SwapchainModule);

    SwapchainModule(GraphModuleManager *gmm):GraphModule(gmm,"Swapchain"){}
    virtual ~SwapchainModule()=default;

};//class SwapchainModule:public RenderModule

VK_NAMESPACE_END
