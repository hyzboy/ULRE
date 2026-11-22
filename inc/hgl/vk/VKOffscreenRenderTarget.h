#pragma once

#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetData.h>
#include<hgl/vk/DebugCaptureQueue.h>
#include<hgl/vk/VKDevice.h>

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
    uint64_t debug_capture_target=0;
    uint64_t debug_capture_frame=0;
    bool debug_recording=false;
    bool debug_recorded=false;
    bool debug_managed_pass=false;
    bool debug_managed_opt_in=false;
    float debug_clear_depth=0.0f;

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
        if(debug_capture_target)
            DebugCaptureQueue::UnregisterTarget(debug_capture_target);
        if(data)
        {
            data->Clear();
            delete data;
        }
    }

    void RegisterDebugCapture(const char *name=nullptr)
    {
        Texture2D *color=GetColorCount()?GetColorTexture(0):nullptr;
        Texture2D *depth=GetDepthTexture();
        const bool supported_count=GetColorCount()<=1;
        const bool can_color=debug_managed_opt_in && supported_count && color
                             && (color->GetUsage()&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                             && DebugCaptureQueue::SupportsFormat(color->GetFormat(),DebugCaptureQueue::Attachment::Color);
        const bool can_depth=debug_managed_opt_in && supported_count && depth
                             && (depth->GetUsage()&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                             && DebugCaptureQueue::SupportsFormat(depth->GetFormat(),DebugCaptureQueue::Attachment::Depth);
        if(!debug_capture_target)
            debug_capture_target=DebugCaptureQueue::RegisterTarget(
                false,false,DebugCaptureQueue::Source::Offscreen,this,name);
        VkFormat colors[8]{};
        const uint32_t count=GetColorCount();
        for(uint32_t i=0;i<count && i<8;++i)
        {
            Texture2D *texture=GetColorTexture(i);
            colors[i]=texture?texture->GetFormat():VK_FORMAT_UNDEFINED;
        }
        DebugCaptureQueue::SetOffscreenDetails(debug_capture_target,colors,count,
                                               depth?depth->GetFormat():VK_FORMAT_UNDEFINED,
                                               can_color,can_depth);
    }

    bool HasDebugCapture() const { return debug_capture_target!=0; }
    uint64_t GetDebugCaptureTargetID() const { return debug_capture_target; }

    // The owner guarantees managed writes and graphics-queue-only access to these attachments.
    void EnableManagedDebugCapture()
    {
        if(!debug_capture_target)
            return;
        debug_managed_opt_in=true;
        RegisterDebugCapture();
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

        if(!debug_recorded)
        {
            if(debug_capture_target)
                DebugCaptureQueue::FailQueued(debug_capture_target,"recording_failed");
            return false;
        }
        const bool submitted=data->Submit(extra_waits,extra_wait_count);
        if(debug_capture_target)
        {
            DeviceQueue *queue=data->GetQueue();
            const bool same_queue=queue && data->device
                && static_cast<VkQueue>(*queue)==data->device->GetGraphicsQueue();
            Texture2D *color=GetColorCount()?GetColorTexture(0):nullptr;
            Texture2D *depth=GetDepthTexture();
            const bool layouts=(!color || color->GetImageLayout()==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                            && (!depth || depth->GetImageLayout()==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if(!submitted || !debug_managed_opt_in || !debug_managed_pass || !same_queue
               || !layouts || debug_capture_frame==UINT64_MAX)
                DebugCaptureQueue::FailQueued(debug_capture_target,
                    submitted?"frame_unavailable":"submission_failed");
            else
            {
                ++debug_capture_frame;
                DebugCaptureQueue::BeginFrame(debug_capture_target,debug_capture_frame);
                DebugCaptureQueue::RecordDepthClear(debug_capture_target,hasDepth(),debug_clear_depth);
                DebugCaptureQueue::Process(this,debug_capture_target,debug_capture_frame);
            }
        }
        debug_recorded=false;
        debug_managed_pass=false;
        return submitted;
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

        RenderCmdBuffer *cmd=data->BeginRender();
        debug_recording=cmd!=nullptr;
        debug_recorded=false;
        debug_managed_pass=false;
        if(!cmd && debug_capture_target)
            DebugCaptureQueue::FailQueued(debug_capture_target,"recording_failed");
        return cmd;
    }

    virtual void EndRender() override
    {
        if(!data)
            return;

        debug_recorded=debug_recording && data->EndRender();
        if(!debug_recorded && debug_capture_target)
            DebugCaptureQueue::FailQueued(debug_capture_target,"recording_failed");
        else if(debug_capture_target && debug_managed_opt_in && !debug_managed_pass)
            DebugCaptureQueue::FailQueued(debug_capture_target,"frame_unavailable");
        debug_recording=false;
    }

    void MarkManagedCapturePass(bool safe,float clear_depth) override
    {
        debug_managed_pass=debug_recording && safe;
        debug_clear_depth=clear_depth;
    }
    bool IsRenderRecordingValid() const override { return debug_recorded; }
};//class OffscreenRenderTarget

}//namespace hgl::graph
