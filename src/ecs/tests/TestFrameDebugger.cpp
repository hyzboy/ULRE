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
    if(!DebugCaptureQueue::SupportsFormat(VK_FORMAT_R8G8B8A8_UNORM,DebugCaptureQueue::Attachment::Color)
       || !DebugCaptureQueue::SupportsFormat(VK_FORMAT_D32_SFLOAT,DebugCaptureQueue::Attachment::Depth)
       || DebugCaptureQueue::SupportsFormat(VK_FORMAT_S8_UINT,DebugCaptureQueue::Attachment::Depth)
       || DebugCaptureQueue::SupportsFormat(VK_FORMAT_UNDEFINED,DebugCaptureQueue::Attachment::Color))
        return 2;
    EmptyTarget other_target;
    const uint64_t unavailable=DebugCaptureQueue::RegisterTarget(
        false,false,DebugCaptureQueue::Source::Offscreen,&other_target,"unavailable");
    const char *unavailable_error=nullptr;
    if(!::strstr(DebugCaptureQueue::GetTargetsJSON().c_str(),"\"availability\":\"unavailable\"")
       || DebugCaptureQueue::Request(unavailable,DebugCaptureQueue::Attachment::Color,&unavailable_error)
       || !unavailable_error || ::strcmp(unavailable_error,"attachment_unavailable")!=0)
        return 2;
    DebugCaptureQueue::UnregisterTarget(unavailable);
    const uint64_t offscreen=DebugCaptureQueue::RegisterTarget(
        true,false,DebugCaptureQueue::Source::Offscreen,&target);
    if(!::strstr(DebugCaptureQueue::GetTargetsJSON().c_str(),"\"source\":\"offscreen\""))
        return 2;
    const uint64_t wrong_target=DebugCaptureQueue::Request(offscreen,DebugCaptureQueue::Attachment::Color);
    if(!DebugCaptureQueue::HasQueued(offscreen)) return 2;
    DebugCaptureQueue::Process(&other_target,offscreen,1);
    DebugCaptureQueue::Result result;
    if(!DebugCaptureQueue::GetResult(wrong_target,result)
       || result.state!=DebugCaptureQueue::State::Failed || !result.error
       || ::strcmp(result.error,"target_mismatch")!=0
       || !DebugCaptureQueue::Release(wrong_target)
       || DebugCaptureQueue::HasQueued(offscreen))
        return 2;
    const uint64_t rebuilt=DebugCaptureQueue::Request(offscreen,DebugCaptureQueue::Attachment::Color);
    const VkFormat formats[]={VK_FORMAT_R8G8B8A8_UNORM};
    DebugCaptureQueue::SetOffscreenDetails(offscreen,formats,1,VK_FORMAT_D32_SFLOAT,false,true);
    if(!DebugCaptureQueue::GetResult(rebuilt,result) || result.state!=DebugCaptureQueue::State::Failed
       || !result.error || ::strcmp(result.error,"target_rebuilt")!=0
       || !DebugCaptureQueue::Release(rebuilt)
       || DebugCaptureQueue::Request(offscreen,DebugCaptureQueue::Attachment::Color))
        return 2;
    if(!::strstr(DebugCaptureQueue::GetTargetsJSON().c_str(),"\"color_attachment_count\":1")
       || !::strstr(DebugCaptureQueue::GetTargetsJSON().c_str(),"\"attachment\":\"depth\""))
        return 2;
    DebugCaptureQueue::UnregisterTarget(offscreen);
    const uint64_t no_depth=DebugCaptureQueue::RegisterTarget(true,false);
    const char *error=nullptr;
    if(!no_depth || DebugCaptureQueue::Request(no_depth,DebugCaptureQueue::Attachment::Depth,&error)
       || !error || ::strcmp(error,"attachment_unavailable")!=0)
        return 2;
    if(DebugCaptureQueue::Request(no_depth,DebugCaptureQueue::Attachment::ColorDepth,&error)
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
    DebugCaptureQueue::RecordDepthClear(target_id,true,0.25f);
    if(!target_id || DebugCaptureQueue::Request(0,DebugCaptureQueue::Attachment::Color,&error)
       || !error || ::strcmp(error,"unknown_target")!=0)
        return 2;
    const uint64_t request_id=DebugCaptureQueue::Request(target_id,DebugCaptureQueue::Attachment::Color);
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
       || !result.clear_depth_valid || result.clear_depth!=0.25f
       || !result.error || ::strcmp(result.error,"readback_failed")!=0
       || !DebugCaptureQueue::Release(request_id)
       || DebugCaptureQueue::GetResult(request_id,result))
        return 4;

    const uint64_t debug_target=DebugCaptureQueue::RegisterTarget(
        true,true,DebugCaptureQueue::Source::DebugView);
    DebugCaptureQueue::BeginFrame(debug_target,77,1);
    if(!DebugCaptureQueue::CopyCamera(target_id,debug_target,77)
       || DebugCaptureQueue::CopyCamera(target_id,debug_target,76))
        return 4;
    const uint64_t copied=DebugCaptureQueue::Request(debug_target,DebugCaptureQueue::Attachment::Color);
    DebugCaptureQueue::Process(&target,debug_target,77);
    if(!DebugCaptureQueue::GetResult(copied,result) || !result.camera.valid
       || result.source!=DebugCaptureQueue::Source::DebugView || result.debug_mode!=1
       || !DebugCaptureQueue::Release(copied))
        return 4;

    DebugCaptureQueue::BeginFrame(target_id,78);
    DebugCaptureQueue::RecordDepthClear(target_id,true,0.75f);
    DebugCaptureQueue::RecordDepthClear(target_id,false,0.0f);
    DebugCaptureQueue::RecordCamera(target_id,&camera);
    camera.vp[0][0]=2.0f;
    DebugCaptureQueue::RecordCamera(target_id,&camera);
    DebugCaptureQueue::BeginFrame(debug_target,78,1);
    if(DebugCaptureQueue::CopyCamera(target_id,debug_target,78))
        return 4;
    const uint64_t failed=DebugCaptureQueue::Request(debug_target,DebugCaptureQueue::Attachment::Color);
    DebugCaptureQueue::FailQueued(debug_target,"unsupported_scene_batch");
    if(!DebugCaptureQueue::GetResult(failed,result) || result.state!=DebugCaptureQueue::State::Failed
       || !result.error || ::strcmp(result.error,"unsupported_scene_batch")!=0
       || !DebugCaptureQueue::Release(failed))
        return 4;
    DebugCaptureQueue::UnregisterTarget(debug_target);
    const uint64_t ambiguous=DebugCaptureQueue::Request(target_id,DebugCaptureQueue::Attachment::Color);
    DebugCaptureQueue::Process(&target,target_id,78);
    if(!DebugCaptureQueue::GetResult(ambiguous,result) || result.camera.valid
       || !result.camera.ambiguous || result.clear_depth_valid || !DebugCaptureQueue::Release(ambiguous))
        return 4;
    const uint64_t group=DebugCaptureQueue::Request(target_id,DebugCaptureQueue::Attachment::ColorDepth);
    DebugCaptureQueue::Process(&target,target_id,78);
    if(!DebugCaptureQueue::GetResult(group,result) || result.state!=DebugCaptureQueue::State::Failed
       || result.path[0] || result.depth.path[0] || !DebugCaptureQueue::Release(group))
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

    // 结构化 JSON 输出与转义回归（旧手工拼接实现不转义实体名/详情）
    FrameDebugger::PipelineInspectionSnapshot snap{};
    snap.valid=true;
    snap.frame_serial=5;
    snap.scene_primitives=10;
    snap.collected_items=8;
    snap.fast_path_engaged=true;
    snap.drop_sample_count=1;
    ::strcpy(snap.drop_samples[0].entity_name,"ch\"ess");
    snap.drop_samples[0].entity_id=7;
    ::strcpy(snap.drop_samples[0].stage,"Collect");
    ::strcpy(snap.drop_samples[0].reason,"invisible");
    ::strcpy(snap.drop_samples[0].detail,"a\\b");
    FrameDebugger::SetPipelineInspection(snap);
    const hgl::AnsiString inspection=FrameDebugger::GetPipelineInspectionJSON();
    if(!::strstr(inspection.c_str(),"\"funnel\":{")
       || !::strstr(inspection.c_str(),"\"cache_state\":{")
       || !::strstr(inspection.c_str(),"ch\\\"ess")
       || !::strstr(inspection.c_str(),"a\\\\b"))
    {
        GLogError(u8"Pipeline inspection JSON lost structure or escaping");
        return 8;
    }
    const hgl::AnsiString dropped=FrameDebugger::TraceEntityPipelineJSON("ch\"ess");
    if(!::strstr(dropped.c_str(),"\"status\":\"Dropped\"")
       || !::strstr(dropped.c_str(),"ch\\\"ess"))
        return 8;
    if(!::strstr(FrameDebugger::TraceEntityPipelineJSON("7").c_str(),"\"status\":\"Dropped\""))
        return 8;
    if(!::strstr(FrameDebugger::TraceEntityPipelineJSON("absent").c_str(),"RenderedOrPassed"))
        return 8;
    FrameDebugger::SetCameraSnapshot(&camera);
    const hgl::AnsiString camera_json=FrameDebugger::GetCameraSnapshotJSON();
    if(!::strstr(camera_json.c_str(),"\"vp\":[")
       || !::strstr(camera_json.c_str(),"\"use_reversed_z\":1"))
        return 8;
    return 0;
}
