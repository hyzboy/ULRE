#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    class ECSContext;
}

namespace hgl::graph{

GRAPH_MODULE_CLASS(SwapchainModule)
{
    OBJECT_LOGGER

    TextureManager *        tex_manager         =nullptr;
    RenderTargetManager *   rt_manager          =nullptr;
    RenderPassManager *     rp_manager          =nullptr;
    hgl::ecs::ECSContext *  ecs_context         =nullptr;

    RenderPass *            sc_render_pass      =nullptr;
    SwapchainRenderTarget * sc_render_target    =nullptr;

protected:

    bool        CreateSwapchainFBO(Swapchain *);
    Swapchain * CreateSwapchain();
    bool        CreateSwapchainRenderTarget();

public:

    virtual void OnResize(const VkExtent2D &)override;

public:

    SwapchainModule(GraphicsContext *gc,hgl::ecs::ECSContext *ecs_ctx,TextureManager *tm,RenderTargetManager *rtm,RenderPassManager *rpm);
    virtual ~SwapchainModule();

    void Release() override;

public:

    RenderPass *            GetRenderPass   ()const{return sc_render_pass;}
    SwapchainRenderTarget * GetRenderTarget ()const{return sc_render_target;}
    bool                    GetSwapchainSize(VkExtent2D *)const;
    bool                    AcquireNextImage()const;

};//class SwapchainModule:public GraphModule

}//namespace hgl::graph
