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

    DeviceQueue *       GetQueue            ()override{return data ? data->GetQueue() : nullptr;}

    /// 本 RT 的车道（离屏 pass 的完成时点，主帧提交 await 它）
    Semaphore *         GetLane             ()override{return data ? data->GetLane() : nullptr;}
    uint64_t            GetLaneValue        ()const override{return data ? data->GetLaneValue() : 0;}

    RenderCmdBuffer *   GetRenderCmdBuffer  ()override{return data ? data->GetCmdBuffer() : nullptr;}

    /// 提交：extra_waits 由调用方给出（主帧车道、上传完成等）
    virtual bool        Submit              (const SemaphoreSubmit *extra_waits,const uint32_t extra_wait_count)override
    {
        if(!data)
            return(false);

        return data->Submit(extra_waits,extra_wait_count);
    }

    /// per-frame 数据槽号（L2W ring / CameraInfo 行 / Viewport 槽按它取号）
    uint32_t            GetCurrentFrameIndex()const override{return data ? data->GetCurrentFrameIndex() : 0;}

    /// per-frame 数据槽总数（全局，主帧 + 离屏共用一段索引空间）
    uint32_t            GetFrameCount       ()const override{return data ? data->GetFrameCount() : 1;}

    bool                WaitQueue           ()override
    {
        DeviceQueue *q = GetQueue();
        return q ? q->WaitQueue() : false;
    }

    /// 全槽排空：等本 RT 所有在途槽完成（resize / 生命周期 / 帧间同步点用）。
    /// 逐槽的复用等待在 RenderTargetData::BeginRender 内完成，无需调用方参与。
    bool                WaitFence           ()override
    {
        if(!data || !data->queues)
            return(false);

        bool ok = true;
        for(uint32_t i = 0; i < data->slot_count; i++)
            if(data->queues[i] && !data->queues[i]->WaitLastSubmitFence())
                ok = false;

        return ok;
    }

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
