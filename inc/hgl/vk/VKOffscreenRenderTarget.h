#pragma once

#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetData.h>

namespace hgl::graph{

/**
 * 离屏（单帧）渲染目标
 *
 * 与 SwapchainRenderTarget 对称：
 * - OffscreenRenderTarget —— 离屏纹理，由 RenderTargetManager 创建并持有
 * - SwapchainRenderTarget —— 窗口交换链，由 SwapchainModule 创建
 *
 * 文件名原为 VKRenderTargetSingle.h，类名原为 RenderTarget；
 * 标准化阶段 B 起统一为 OffscreenRenderTarget（旧名以别名形式保留，见 VK.h）。
 */
class OffscreenRenderTarget:public IRenderTarget
{
    RenderTargetData *data;

protected:

    friend class SwapchainModule;
    friend class RenderTargetManager;

    OffscreenRenderTarget(hgl::ecs::ECSContext *ctx,RenderTargetData *rtd):IRenderTarget(ctx,rtd->fbo->GetExtent())
    {
        data=rtd;
    }

public:

    virtual ~OffscreenRenderTarget() override
    {
        if(data)
        {
            data->Clear();
            delete data;
        }
    }

    Framebuffer *       GetFramebuffer      ()override{return data->fbo;}
    RenderPass *        GetRenderPass       ()override{return data->fbo->GetRenderPass();}

    uint32_t            GetColorCount       ()override{return data->color_count;}

    bool                hasDepth            ()override{return data->depth_texture;}

    Texture2D *         GetColorTexture     (const int index=0) override{return data->GetColorTexture(index);}
    Texture2D *         GetDepthTexture     ()                  override{return data->depth_texture;}

public: // Command Buffer

    DeviceQueue *       GetQueue            ()override{return data->queue;}
    Semaphore *         GetRenderCompleteSemaphore()override{return data->render_complete_semaphore;}

    RenderCmdBuffer *   GetRenderCmdBuffer  ()override{return data->cmd_buf;}

    virtual bool        Submit              (Semaphore *wait_sem)override
    {
        if(!data)
            return(false);

        return data->Submit(wait_sem);
    }

    bool                WaitQueue           ()override{return data->queue->WaitQueue();}
    bool                WaitFence           ()override{return data->queue->WaitLastSubmitFence();}

public:

    virtual RenderCmdBuffer *BeginRender()override
    {
        if(!data)
            return(nullptr);

        return data->BeginRender();
    }

    virtual void EndRender() override
    {
        if(!data)
            return;

        data->EndRender();
    }
};//class OffscreenRenderTarget

}//namespace hgl::graph
