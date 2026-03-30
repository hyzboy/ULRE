#pragma once

#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetData.h>

namespace hgl::graph{

/**
 * 单帧渲染目标
 */
class RenderTarget:public IRenderTarget
{
    RenderTargetData *data;

protected:

    friend class SwapchainModule;
    friend class RenderTargetManager;

    RenderTarget(hgl::ecs::ECSContext *ctx,RenderTargetData *rtd):IRenderTarget(ctx,rtd->extent)
    {
        data=rtd;
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

    RenderPass *        GetRenderPass       ()override{return data->render_pass;}

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

    virtual RenderTargetData* GetCurrentRTD() override { return data; }

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
};//class RenderTarget

}//namespace hgl::graph
