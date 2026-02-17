#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/vk/VKFrameData.h>
#include<hgl/vk/VKSwapchainData.h>

namespace hgl::ecs
{
    class ECSContext;
}

VK_NAMESPACE_BEGIN

// Forward declarations
struct SwapchainData;

GRAPH_MODULE_CLASS(SwapchainModule)
{
    TextureManager *        tex_manager         =nullptr;
    RenderTargetManager *   rt_manager          =nullptr;
    RenderPassManager *     rp_manager          =nullptr;
    hgl::ecs::ECSContext *  ecs_context         =nullptr;

    // New architecture: SwapchainData owns swapchain and frame resources
    SwapchainData *         swapchain_data      =nullptr;

    // Legacy support (will be deprecated)
    RenderPass *            sc_render_pass      =nullptr;
    SwapchainRenderTarget * sc_render_target    =nullptr;

protected:

    bool CreateSwapchainFBO(Swapchain *);
    Swapchain *CreateSwapchain();
    bool CreateSwapchainRenderTarget();

    // New methods for cleaner architecture
    bool CreatePerFrameResources(SwapchainData &sc_data);
    bool DestroyPerFrameResources(SwapchainData &sc_data);

public:

    virtual void OnResize(const VkExtent2D &)override;                                              ///<窗口大小改变

public:

    SwapchainModule(GraphicsContext *gc,hgl::ecs::ECSContext *ecs_ctx,TextureManager *tm,RenderTargetManager *rtm,RenderPassManager *rpm);
    virtual ~SwapchainModule();

    // New Architecture Methods
    bool Initialize();                                                      ///< Initialize swapchain with new architecture
    
    FrameResources *        GetCurrentFrame()  const;                      ///< Get current frame resources
    FrameResources *        GetFrame(uint32_t index) const;                ///< Get frame resources by index
    SwapchainData *         GetSwapchainData() const {return swapchain_data;}  ///< Get swapchain data container

    void Release() override;

public:

    // Legacy methods (@deprecated - kept for backward compatibility)
    RenderPass *            GetRenderPass   ()const{return sc_render_pass;}

    bool                    GetSwapchainSize(VkExtent2D *)const;

    SwapchainRenderTarget * GetRenderTarget ()const{return sc_render_target;}
    bool                    AcquireNextImage()const;
};//class SwapchainModule:public GraphModule

VK_NAMESPACE_END
