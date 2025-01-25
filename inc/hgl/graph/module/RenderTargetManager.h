#pragma once

#include<hgl/graph/module/GraphModule.h>

VK_NAMESPACE_BEGIN

class TextureManager;
class RenderPassManager;

GRAPH_MODULE_CLASS(RenderTargetManager)
{
    TextureManager *tex_manager;
    RenderPassManager *rp_manager;

public:

    RenderTargetManager(GPUDevice *,TextureManager *tm,RenderPassManager *rpm);
    virtual ~RenderTargetManager()=default;

public: //FrameBuffer相关

    Framebuffer *CreateFBO(RenderPass *rp,ImageView **color_list,const uint image_count,ImageView *depth);
//    Framebuffer *CreateFBO(RenderPass *,List<ImageView *> &color,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *color,ImageView *depth);
    Framebuffer *CreateFBO(RenderPass *,ImageView *);

public:
    
    RenderTarget *CreateRT(   const FramebufferInfo *fbi,RenderPass *,const uint32_t fence_count=1);
    RenderTarget *CreateRT(   const FramebufferInfo *fbi,const uint32_t fence_count=1);
};//class RenderTargetManager

VK_NAMESPACE_END
