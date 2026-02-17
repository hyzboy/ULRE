#pragma once

#include<hgl/graph/module/GraphModule.h>

namespace hgl::ecs
{
    class ECSContext;
}

VK_NAMESPACE_BEGIN

class TextureManager;
class RenderPassManager;
class GraphicsContext;

GRAPH_MODULE_CLASS(RenderTargetManager)
{
    TextureManager *tex_manager;
    RenderPassManager *rp_manager;
    hgl::ecs::ECSContext *ecs_context=nullptr;

public:

    RenderTargetManager(GraphicsContext *gc,hgl::ecs::ECSContext *ecs_ctx,TextureManager *tm,RenderPassManager *rpm);
    virtual ~RenderTargetManager()=default;

public: //FrameBuffer相关

    Framebuffer *CreateFBO(RenderPass *rp,ImageView **color_list,const uint image_count,ImageView *depth);
//    Framebuffer *CreateFBO(RenderPass *,ValueArray<ImageView *> &color,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *color,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *);

public:

    RenderTarget *CreateRT(const AnsiString &name, const FramebufferInfo *fbi,RenderPass *,const uint32_t fence_count=1);
    RenderTarget *CreateRT(const AnsiString &name, const FramebufferInfo *fbi,const uint32_t fence_count=1);

    // Create an offscreen render target without RenderFramework (ECS/GraphicsContext path).
    static RenderTarget *CreateRTFromGraphicsContext(GraphicsContext *gc, hgl::ecs::ECSContext *ecs_ctx,
                                                     const FramebufferInfo *fbi, const uint32_t fence_count=1);
    static RenderTarget *CreateRTFromGraphicsContext(GraphicsContext *gc, hgl::ecs::ECSContext *ecs_ctx,
                                                     const AnsiString &name, const FramebufferInfo *fbi, const uint32_t fence_count=1);

    void Release() override
    {
        // RenderTargetManager 通常不直接管理资源的所有权，而是通过关联的管理器
        // 这里的 Release() 是为了遵循统一的 Release() 模式
        // 具体的资源清理由相关的 TextureManager, RenderPassManager 等完成
    }
};//class RenderTargetManager

VK_NAMESPACE_END
