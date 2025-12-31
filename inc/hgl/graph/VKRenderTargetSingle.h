#pragma once

#include<hgl/graph/VKRenderTarget.h>
#include<hgl/graph/VKRenderTargetData.h>

VK_NAMESPACE_BEGIN

/**
 * 单帧渲染目标
 */
class RenderTarget:public IRenderTarget
{
    RenderTargetData *data;

protected:

    friend class SwapchainModule;
    friend class RenderTargetManager;

    RenderTarget(RenderFramework *rf,RenderTargetData *rtd):IRenderTarget(rf,rtd->fbo->GetExtent())
    {
        data=rtd;

        data->cmd_buf->SetDescriptorBinding(GetDescriptorBinding());
    }

public:

    virtual ~RenderTarget() override
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
    bool                WaitFence           ()override{return data->queue->WaitFence();}

public:

    virtual RenderCmdBuffer *BeginRender()override
    {
        if(!data)
            return(nullptr);

        return data->BeginRender(GetDescriptorBinding());
    }

    virtual void EndRender() override
    {
        if(!data)
            return;

        data->EndRender();
    }
};//class RenderTarget

VK_NAMESPACE_END
