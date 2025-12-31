#pragma once

#include<hgl/graph/module/GraphModule.h>

VK_NAMESPACE_BEGIN

GRAPH_MODULE_CLASS(SwapchainModule)
{
    TextureManager *        tex_manager =nullptr;
    RenderTargetManager *   rt_manager  =nullptr;
    RenderPassManager *     rp_manager  =nullptr;

    RenderPass *            sc_render_pass  =nullptr;

    SwapchainRenderTarget * sc_render_target=nullptr;

protected:

    bool CreateSwapchainFBO(Swapchain *);
    Swapchain *CreateSwapchain();
    bool CreateSwapchainRenderTarget();

public:

    virtual void OnResize(const VkExtent2D &)override;                                              ///<窗口大小改变

public:

    SwapchainModule(RenderFramework *,TextureManager *tm,RenderTargetManager *rtm,RenderPassManager *rpm);
    virtual ~SwapchainModule();

//    RenderCmdBuffer *BeginRender();

    //void EndRender();

public:

            RenderPass *            GetRenderPass   ()const{return sc_render_pass;}

            bool                    GetSwapchainSize(VkExtent2D *)const;

            SwapchainRenderTarget * GetRenderTarget ()const{return sc_render_target;}
            bool                    AcquireNextImage()const;
};//class SwapchainModule:public GraphModule

VK_NAMESPACE_END
