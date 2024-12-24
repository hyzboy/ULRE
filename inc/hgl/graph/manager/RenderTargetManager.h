#pragma once

#include<hgl/graph/module/GraphModule.h>

VK_NAMESPACE_BEGIN

class RenderTargetManager:public GraphModule
{
public:

    GRAPH_MODULE_CONSTRUCT(RenderTargetManager)
    virtual ~RenderTargetManager();

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
