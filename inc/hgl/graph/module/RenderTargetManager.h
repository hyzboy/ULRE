#pragma once

#include<hgl/graph/module/GraphModule.h>

namespace hgl::ecs
{
    class ECSContext;
}

VK_NAMESPACE_BEGIN

class TextureManager;
class RenderPassManager;
class IGraphicsContext;

GRAPH_MODULE_CLASS(RenderTargetManager)
{
    TextureManager *tex_manager;
    RenderPassManager *rp_manager;
    hgl::ecs::ECSContext *ecs_context=nullptr;

public:

    RenderTargetManager(RenderFramework *,TextureManager *tm,RenderPassManager *rpm);
    virtual ~RenderTargetManager()=default;

public: //FrameBuffer相关

    Framebuffer *CreateFBO(RenderPass *rp,ImageView **color_list,const uint image_count,ImageView *depth);
//    Framebuffer *CreateFBO(RenderPass *,ValueArray<ImageView *> &color,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *color,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *);

public:

    RenderTarget *CreateRT(   const FramebufferInfo *fbi,RenderPass *,const uint32_t fence_count=1);
    RenderTarget *CreateRT(   const FramebufferInfo *fbi,const uint32_t fence_count=1);

    // Create an offscreen render target without RenderFramework (ECS/GraphicsContext path).
    static RenderTarget *CreateRTFromGraphicsContext(IGraphicsContext *gc, hgl::ecs::ECSContext *ecs_ctx,
                                                     const FramebufferInfo *fbi, const uint32_t fence_count=1);
};//class RenderTargetManager

VK_NAMESPACE_END
