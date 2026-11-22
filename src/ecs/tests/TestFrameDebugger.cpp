#include<hgl/vk/FrameDebugger.h>
#include<hgl/vk/DebugCaptureQueue.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/log/Log.h>
#include<hgl/time/Time.h>
#include<cstring>

namespace
{
    class EmptyTarget final : public hgl::graph::IRenderTarget
    {
    public:
        EmptyTarget():IRenderTarget(nullptr,VkExtent2D{1,1}){}
        uint32_t GetColorCount() override { return 1; }
        bool hasDepth() override { return false; }
        hgl::graph::Framebuffer *GetFramebuffer() override { return nullptr; }
        hgl::graph::RenderPass *GetRenderPass() override { return nullptr; }
        hgl::graph::Texture2D *GetColorTexture(int) override { return nullptr; }
        hgl::graph::Texture2D *GetDepthTexture() override { return nullptr; }
        hgl::graph::DeviceQueue *GetQueue() override { return nullptr; }
        hgl::graph::RenderCmdBuffer *GetRenderCmdBuffer() override { return nullptr; }
        bool Submit(const hgl::graph::SemaphoreSubmit *,uint32_t) override { return false; }
        bool WaitQueue() override { return false; }
        bool WaitFence() override { return false; }
        hgl::graph::RenderCmdBuffer *BeginRender() override { return nullptr; }
        void EndRender() override {}
    };
}

int main()
{
    using hgl::graph::FrameDebugger;
    EmptyTarget target;
    if(FrameDebugger::RequestColor(&target,"../unsafe")!=UINT32_MAX)
    {
        GLogError(u8"Unsafe capture name was accepted");
        return 1;
    }
    if(FrameDebugger::RequestColor(&target,"missing") == UINT32_MAX)
    {
        GLogError(u8"Valid capture request was rejected");
        return 1;
    }
    if(FrameDebugger::CollectPending()!=0)
    {
        GLogError(u8"Empty target was reported as a successful capture");
        return 1;
    }
    const FrameDebugger::CaptureResult *first=FrameDebugger::GetResult(0);
    if(!first || first->valid || !first->error
       || ::strcmp(first->error,"readback_failed")!=0)
    {
        GLogError(u8"Readback failure was not recorded");
        return 1;
    }
    const uint64_t first_id=first->capture_id;
    if(FrameDebugger::RequestColor(&target,"missing2")==UINT32_MAX
       || FrameDebugger::CollectPending()!=0)
        return 1;
    const FrameDebugger::CaptureResult *second=FrameDebugger::GetResult(1);
    if(!second || second->capture_id<=first_id
       || !::strstr(FrameDebugger::GetLatestResultJSON().c_str(),"readback_failed"))
    {
        GLogError(u8"Latest failure did not identify the new capture");
        return 1;
    }
    using hgl::graph::DebugCaptureQueue;
    const uint64_t no_depth=DebugCaptureQueue::RegisterTarget(true,false);
    const char *error=nullptr;
    if(!no_depth || DebugCaptureQueue::Request(no_depth,DebugCaptureQueue::Attachment::Depth,&error)
       || !error || ::strcmp(error,"attachment_unavailable")!=0)
        return 2;
    DebugCaptureQueue::UnregisterTarget(no_depth);
    const uint64_t target_id=DebugCaptureQueue::RegisterTarget();
    hgl::graph::CameraInfo camera{};
    for(int column=0;column<4;++column)
        for(int row=0;row<4;++row)
            camera.vp[column][row]=camera.inverse_vp[column][row]=(column==row)?1.0f:0.0f;
    camera.use_reversed_z=1;
    DebugCaptureQueue::BeginFrame(target_id,77);
    DebugCaptureQueue::RecordCamera(target_id,&camera);
    if(!target_id || DebugCaptureQueue::Request(0,DebugCaptureQueue::Attachment::Color,&error)
       || !error || ::strcmp(error,"unknown_target")!=0)
        return 2;
    const uint64_t request_id=DebugCaptureQueue::Request(target_id,DebugCaptureQueue::Attachment::Color);
    DebugCaptureQueue::Result result;
    if(!request_id || !DebugCaptureQueue::GetResult(request_id,result)
       || result.state!=DebugCaptureQueue::State::Queued || DebugCaptureQueue::Release(request_id))
        return 3;
    const uint64_t cancelled=DebugCaptureQueue::Request(target_id,DebugCaptureQueue::Attachment::Depth);
    if(!cancelled || !DebugCaptureQueue::Cancel(cancelled)
       || !DebugCaptureQueue::GetResult(cancelled,result)
       || result.state!=DebugCaptureQueue::State::Cancelled
       || !DebugCaptureQueue::Release(cancelled))
        return 3;
    const uint64_t expiring=DebugCaptureQueue::Request(target_id,DebugCaptureQueue::Attachment::Depth,nullptr,1);
    hgl::SleepSecond(0.01);
    if(!expiring || !DebugCaptureQueue::GetResult(expiring,result)
       || result.state!=DebugCaptureQueue::State::Failed
       || !result.error || ::strcmp(result.error,"queue_timeout")!=0
       || !DebugCaptureQueue::Release(expiring))
        return 3;
    DebugCaptureQueue::Process(&target,target_id,77);
    if(!DebugCaptureQueue::GetResult(request_id,result)
       || result.state!=DebugCaptureQueue::State::Failed || result.frame_id!=77
       || !result.camera.valid || result.camera.vp[0]!=1.0f || result.camera.reversed_z!=1
       || !result.error || ::strcmp(result.error,"readback_failed")!=0
       || !DebugCaptureQueue::Release(request_id)
       || DebugCaptureQueue::GetResult(request_id,result))
        return 4;

    DebugCaptureQueue::BeginFrame(target_id,78);
    DebugCaptureQueue::RecordCamera(target_id,&camera);
    camera.vp[0][0]=2.0f;
    DebugCaptureQueue::RecordCamera(target_id,&camera);
    const uint64_t ambiguous=DebugCaptureQueue::Request(target_id,DebugCaptureQueue::Attachment::Color);
    DebugCaptureQueue::Process(&target,target_id,78);
    if(!DebugCaptureQueue::GetResult(ambiguous,result) || result.camera.valid
       || !result.camera.ambiguous || !DebugCaptureQueue::Release(ambiguous))
        return 4;

    uint64_t pending[16]{};
    for(uint64_t &id:pending)
        if(!(id=DebugCaptureQueue::Request(target_id,DebugCaptureQueue::Attachment::Depth)))
            return 5;
    if(DebugCaptureQueue::Request(target_id,DebugCaptureQueue::Attachment::Color,&error)
       || !error || ::strcmp(error,"queue_full")!=0)
        return 6;
    DebugCaptureQueue::UnregisterTarget(target_id);
    for(uint64_t id:pending)
        if(!DebugCaptureQueue::GetResult(id,result)
           || result.state!=DebugCaptureQueue::State::Cancelled
           || !DebugCaptureQueue::Release(id))
            return 7;
    return 0;
}
