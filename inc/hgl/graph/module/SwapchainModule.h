#pragma once

#include<hgl/graph/module/GraphModule.h>

VK_NAMESPACE_BEGIN

class RenderTargetManager;
class RenderPassManager;
class RenderPass;

GRAPH_MODULE_CLASS(SwapchainModule)
{
    Swapchain *             swapchain   =nullptr;

    TextureManager *        tex_manager =nullptr;
    RenderTargetManager *   rt_manager  =nullptr;
    RenderPassManager *     rp_manager  =nullptr;

    RenderPass *            sc_render_pass  =nullptr;

    RTSwapchain *           sc_render_target=nullptr;

    SwapchainImage *        current_sc_image=nullptr;

protected:

    bool CreateSwapchainFBO();
    bool CreateSwapchain();
    bool CreateSwapchainRenderTarget();

public:

    virtual void OnResize(const VkExtent2D &)override;                                              ///<窗口大小改变

public:

    SwapchainModule(GPUDevice *,TextureManager *tm,RenderTargetManager *rtm,RenderPassManager *rpm);
    virtual ~SwapchainModule();

    RenderCmdBuffer *BeginRender();

    void EndRender();

public:

            RenderPass *        GetRenderPass   ()const{return sc_render_pass;}

    const   VkExtent2D &        GetSwapchainSize()const{return sc_render_target->GetExtent();}

            RTSwapchain *       GetRenderTarget ()const{return sc_render_target;}
};//class SwapchainModule:public GraphModule

VK_NAMESPACE_END
