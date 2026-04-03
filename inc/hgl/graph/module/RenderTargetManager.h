#pragma once

#include<hgl/graph/module/GraphModule.h>

namespace hgl::ecs
{
    class ECSContext;
}

namespace hgl::graph{

class TextureManager;
class GraphicsContext;

GRAPH_MODULE_CLASS(RenderTargetManager)
{
    TextureManager *tex_manager;
    hgl::ecs::ECSContext *ecs_context=nullptr;

public:

    RenderTargetManager(GraphicsContext *gc,hgl::ecs::ECSContext *ecs_ctx,TextureManager *tm);
    virtual ~RenderTargetManager()=default;

public:

    RenderTarget *CreateRT(const AnsiString &name, const FramebufferInfo *fbi,RenderTargetFormat *,const uint32_t fence_count=1);
    RenderTarget *CreateRT(const AnsiString &name, const FramebufferInfo *fbi,const uint32_t fence_count=1);

    // Create an offscreen render target without legacy entry (ECS/GraphicsContext path).
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

}//namespace hgl::graph
