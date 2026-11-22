#include<hgl/vk/FrameDebugger.h>
#include<hgl/vk/VKTextureReadback.h>
#include<hgl/vk/VKBufferReadback.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/io/FileOutputStream.h>
#include<hgl/sync/Mutex.h>
#include<hgl/time/Time.h>
#include<hgl/log/Log.h>
#include "DebugBridgeJson.h"
#include<cstring>
#include<cstdio>

namespace hgl::graph
{
namespace
{
FrameDebugger::CaptureRequest *Requests(){ static FrameDebugger::CaptureRequest requests[FrameDebugger::kMaxRequests]; return requests; }
uint32_t &RequestCount(){ static uint32_t count=0; return count; }

FrameDebugger::CaptureResult *Results(){ static FrameDebugger::CaptureResult results[FrameDebugger::kMaxRequests]; return results; }
uint32_t &ResultCount(){ static uint32_t count=0; return count; }
hgl::Mutex &CaptureMutex(){ static hgl::Mutex mutex; return mutex; }

uint8_t *BufferScratch(){ static uint8_t bytes[FrameDebugger::kMaxBufferBytes]; return bytes; }
hgl::ValueArray<uint8_t> &PixelScratch(){ static hgl::ValueArray<uint8_t> bytes; return bytes; }
FrameDebugger::CameraSnapshot &CameraSnapshotState(){ static FrameDebugger::CameraSnapshot snapshot; return snapshot; }
hgl::Mutex &CameraSnapshotMutex(){ static hgl::Mutex mutex; return mutex; }
FrameDebugger::PipelineInspectionSnapshot &PipelineInspectionState(){ static FrameDebugger::PipelineInspectionSnapshot snapshot; return snapshot; }
hgl::Mutex &PipelineInspectionMutex(){ static hgl::Mutex mutex; return mutex; }
uint64_t &CaptureSequence(){ static uint64_t sequence=0; return sequence; }

void CopyText(char *dst,uint32_t cap,const char *src)
{
    if(!dst || cap==0) return;
    dst[0]=0;
    if(!src) return;
    const uint32_t len=strlen(src);
    const uint32_t copy=(len<cap-1)?len:cap-1;
    memcpy(dst,src,copy);
    dst[copy]=0;
}

bool IsSafeCaptureName(const char *name)
{
    if(!name||!name[0])
        return false;
    for(uint32_t i=0;name[i];++i)
    {
        const char c=name[i];
        if(i>=sizeof(FrameDebugger::CaptureRequest::name)-1
           || !((c>='a'&&c<='z')||(c>='A'&&c<='Z')
                ||(c>='0'&&c<='9')||c=='_'||c=='-'))
            return false;
    }
    return true;
}

bool SaveCaptureRaw(const char *name,const uint8_t *data,uint32_t byte_count,char *dump_path,uint32_t cap)
{
    if(!name||!data||byte_count==0||!dump_path||cap==0)
        return false;

    hgl::filesystem::MakePath(OS_TEXT(".ulre-captures"));

    char filename[320];
    snprintf(filename,sizeof(filename),
             ".ulre-captures/%s_%llu_%llu.raw",
             name,
             static_cast<unsigned long long>(hgl::GetTimeUs()),
             static_cast<unsigned long long>(CaptureSequence()));

    hgl::io::OpenFileOutputStream stream(ToOSString(filename),hgl::io::FileOpenMode::Create);
    if(!stream)
        return false;

    const bool ok=stream->Write(data,byte_count)==byte_count;

    if(!ok)
        return false;

    CopyText(dump_path,cap,filename);
    return true;
}

uint32_t AddRequest(const FrameDebugger::CaptureRequest &request)
{
    hgl::MutexLock lock(&CaptureMutex());
    uint32_t &count=RequestCount();
    if(count>=FrameDebugger::kMaxRequests) return UINT32_MAX;
    Requests()[count]=request;
    return count++;
}
}

uint32_t FrameDebugger::RequestColor(IRenderTarget *target,const char *name,uint32_t color_index)
{
    if(!target||!IsSafeCaptureName(name)) return UINT32_MAX;
    CaptureRequest request;
    request.kind=RequestKind::ColorTarget;
    request.render_target=target;
    request.color_index=color_index;
    CopyText(request.name,sizeof(request.name),name);
    return AddRequest(request);
}

uint32_t FrameDebugger::RequestDepth(IRenderTarget *target,const char *name)
{
    if(!target||!IsSafeCaptureName(name)) return UINT32_MAX;
    CaptureRequest request;
    request.kind=RequestKind::DepthTarget;
    request.render_target=target;
    CopyText(request.name,sizeof(request.name),name);
    return AddRequest(request);
}

uint32_t FrameDebugger::RequestBuffer(DeviceBuffer *buffer,const char *name,uint32_t offset,uint32_t size)
{
    if(!buffer||!IsSafeCaptureName(name)) return UINT32_MAX;
    if(offset>static_cast<uint32_t>(buffer->GetSize())) return UINT32_MAX;
    const uint32_t actual_size=(size==0)?static_cast<uint32_t>(buffer->GetSize()-offset):size;
    if(actual_size==0 || actual_size>kMaxBufferBytes
       || actual_size>static_cast<uint32_t>(buffer->GetSize()-offset)) return UINT32_MAX;

    CaptureRequest request;
    request.kind=RequestKind::Buffer;
    request.buffer=buffer;
    request.buffer_offset=offset;
    request.buffer_size=actual_size;
    CopyText(request.name,sizeof(request.name),name);
    return AddRequest(request);
}

uint32_t FrameDebugger::CollectPending()
{
    hgl::MutexLock lock(&CaptureMutex());
    uint32_t &request_count=RequestCount();
    uint32_t &result_count=ResultCount();
    uint32_t collected=0;

    for(uint32_t i=0;i<request_count;i++)
    {
        const CaptureRequest &request=Requests()[i];
        if(request.kind==RequestKind::None) continue;

        CaptureResult result;
        result.request=request;
        result.capture_id=++CaptureSequence();
        result.error="readback_failed";

        if(request.kind==RequestKind::ColorTarget)
        {
            TextureReadbackInfo info;
            ValueArray<uint8_t> &pixels=PixelScratch();
            if(ReadbackColorTarget(request.render_target,pixels,request.color_index,&info))
            {
                result.valid=true;
                result.width=info.width;
                result.height=info.height;
                result.format=info.format;
                result.pixel_size=info.pixel_size;
                result.byte_count=pixels.GetTotalBytes();
            }
        }
        else if(request.kind==RequestKind::DepthTarget)
        {
            TextureReadbackInfo info;
            ValueArray<uint8_t> &pixels=PixelScratch();
            if(ReadbackDepthTarget(request.render_target,pixels,&info))
            {
                result.valid=true;
                result.width=info.width;
                result.height=info.height;
                result.format=info.format;
                result.pixel_size=info.pixel_size;
                result.byte_count=pixels.GetTotalBytes();

                if(info.format==VK_FORMAT_D32_SFLOAT && info.pixel_size==4)
                {
                    const float *values=reinterpret_cast<const float *>(pixels.GetData());
                    const uint32_t count=pixels.GetCount()/4;
                    double sum=0.0;
                    result.min_value=1.0;
                    result.max_value=0.0;
                    for(uint32_t n=0;n<count;n++)
                    {
                        const float value=values[n];
                        if(value<result.min_value) result.min_value=value;
                        if(value>result.max_value) result.max_value=value;
                        sum+=value;
                    }
                    result.average=count?sum/count:0.0;
                }
            }
        }
        else if(request.kind==RequestKind::Buffer)
        {
            uint8_t *bytes=BufferScratch();
            memset(bytes,0,kMaxBufferBytes);
            if(ReadbackBuffer(request.buffer,request.buffer_offset,request.buffer_size,bytes))
            {
                result.valid=true;
                result.byte_count=request.buffer_size;
            }
        }

        if(result.valid)
        {
            bool saved=false;

            if(request.kind==RequestKind::ColorTarget)
            {
                ValueArray<uint8_t> &pixels=PixelScratch();
                saved=SaveCaptureRaw(request.name,pixels.GetData(),
                               static_cast<uint32_t>(pixels.GetTotalBytes()),
                               result.dump_path,sizeof(result.dump_path));
            }
            else if(request.kind==RequestKind::DepthTarget)
            {
                ValueArray<uint8_t> &pixels=PixelScratch();
                saved=SaveCaptureRaw(request.name,pixels.GetData(),
                               static_cast<uint32_t>(pixels.GetTotalBytes()),
                               result.dump_path,sizeof(result.dump_path));
            }
            else if(request.kind==RequestKind::Buffer)
            {
                saved=SaveCaptureRaw(request.name,BufferScratch(),request.buffer_size,
                               result.dump_path,sizeof(result.dump_path));
            }

            if(!saved)
            {
                result.valid=false;
                result.error="write_failed";
                GLogError(u8"[FrameDebugger] Capture write failed: %s",request.name);
            }
            else
                result.error=nullptr;
        }

        if(result_count>=kMaxRequests)
            result_count=0;
        Results()[result_count++]=result;
        if(result.valid)
            collected++;
        else
            GLogError(u8"[FrameDebugger] Capture failed: %s (%s)",request.name,result.error);

        Requests()[i].kind=RequestKind::None;
    }

    request_count=0;
    return collected;
}

uint32_t FrameDebugger::GetResultCount()
{
    hgl::MutexLock lock(&CaptureMutex());
    return ResultCount();
}

const FrameDebugger::CaptureResult *FrameDebugger::GetResult(uint32_t index)
{
    if(index>=ResultCount()) return nullptr;
    return &Results()[index];
}

AnsiString FrameDebugger::GetLatestResultJSON()
{
    hgl::MutexLock lock(&CaptureMutex());
    const uint32_t count=ResultCount();
    if(count==0)
        return debug_bridge::DebugJsonToText(nlohmann::json{{"valid",false}});
    const CaptureResult &result=Results()[count-1];

    const char *kind="unknown";
    switch(result.request.kind)
    {
        case RequestKind::ColorTarget: kind="color_rt"; break;
        case RequestKind::DepthTarget: kind="depth_rt"; break;
        case RequestKind::Buffer: kind="buffer"; break;
        default: break;
    }

    nlohmann::json json={
        {"valid",result.valid},
        {"name",result.request.name},
        {"kind",kind},
        {"width",result.width},
        {"height",result.height},
        {"format",static_cast<int>(result.format)},
        {"bytes",result.byte_count},
        {"dump_path",result.dump_path},
        {"capture_id",result.capture_id}};
    if(result.error)
        json["error"]=result.error;
    return debug_bridge::DebugJsonToText(json);
}

void FrameDebugger::SetCameraSnapshot(const CameraInfo *camera)
{
    hgl::MutexLock lock(&CameraSnapshotMutex());
    CameraSnapshot &snapshot=CameraSnapshotState();
    snapshot.valid=(camera!=nullptr);
    if(!camera)
        return;

    memcpy(snapshot.inverse_vp,&camera->inverse_vp[0][0],sizeof(snapshot.inverse_vp));
    memcpy(snapshot.vp,&camera->vp[0][0],sizeof(snapshot.vp));
    memcpy(snapshot.inverse_view,&camera->inverse_view[0][0],sizeof(snapshot.inverse_view));
    memcpy(snapshot.inverse_projection,&camera->inverse_projection[0][0],sizeof(snapshot.inverse_projection));
    snapshot.znear=camera->znear;
    snapshot.zfar=camera->zfar;
    snapshot.use_reversed_z=camera->use_reversed_z;
}

FrameDebugger::CameraSnapshot FrameDebugger::GetCameraSnapshot()
{
    hgl::MutexLock lock(&CameraSnapshotMutex());
    return CameraSnapshotState();
}

AnsiString FrameDebugger::GetCameraSnapshotJSON()
{
    const CameraSnapshot snapshot=GetCameraSnapshot();
    if(!snapshot.valid)
        return debug_bridge::DebugJsonToText(nlohmann::json{{"valid",false}});

    const auto matrix=[](const float *m)
    {
        nlohmann::json array=nlohmann::json::array();
        for(uint32_t i=0;i<16;i++)
            array.push_back(m[i]);
        return array;
    };

    return debug_bridge::DebugJsonToText(nlohmann::json{
        {"valid",true},
        {"znear",snapshot.znear},
        {"zfar",snapshot.zfar},
        {"use_reversed_z",snapshot.use_reversed_z},
        {"inverse_vp",matrix(snapshot.inverse_vp)},
        {"vp",matrix(snapshot.vp)},
        {"inverse_view",matrix(snapshot.inverse_view)},
        {"inverse_projection",matrix(snapshot.inverse_projection)}});
}

void FrameDebugger::SetPipelineInspection(const PipelineInspectionSnapshot &snapshot)
{
    hgl::MutexLock lock(&PipelineInspectionMutex());
    PipelineInspectionState() = snapshot;
}

FrameDebugger::PipelineInspectionSnapshot FrameDebugger::GetPipelineInspection()
{
    hgl::MutexLock lock(&PipelineInspectionMutex());
    return PipelineInspectionState();
}

AnsiString FrameDebugger::GetPipelineInspectionJSON()
{
    const PipelineInspectionSnapshot snap=GetPipelineInspection();
    if(!snap.valid)
        return debug_bridge::DebugJsonToText(nlohmann::json{{"valid",false}});

    nlohmann::json samples=nlohmann::json::array();
    for(uint32_t i=0;i<snap.drop_sample_count;++i)
    {
        const auto &s=snap.drop_samples[i];
        samples.push_back({{"name",s.entity_name},
                           {"entity_id",s.entity_id},
                           {"stage",s.stage},
                           {"reason",s.reason},
                           {"detail",s.detail}});
    }

    return debug_bridge::DebugJsonToText(nlohmann::json{
        {"valid",true},
        {"frame_serial",snap.frame_serial},
        {"funnel",{{"scene_primitives",snap.scene_primitives},
                   {"passed_can_render",snap.passed_can_render},
                   {"collected_items",snap.collected_items},
                   {"batched_items",snap.batched_items},
                   {"batch_count",snap.batch_count},
                   {"gpu_draw_count",snap.gpu_draw_count}}},
        {"cache_state",{{"fast_path_engaged",snap.fast_path_engaged},
                        {"last_collect_steady",snap.last_collect_steady},
                        {"has_pending_resources",snap.has_pending_resources},
                        {"snapshot_ready",snap.snapshot_ready},
                        {"snapshot_topology_rev",snap.snapshot_topology_rev},
                        {"world_topology_rev",snap.world_topology_rev},
                        {"snapshot_item_count",snap.snapshot_item_count}}},
        {"drops",{{"invisible",snap.dropped_invisible},
                  {"no_transform",snap.dropped_no_transform},
                  {"mobility_filtered",snap.dropped_mobility},
                  {"resource_pending",snap.dropped_resource_pending},
                  {"geometry_loading",snap.dropped_geometry_loading},
                  {"other",snap.dropped_other},
                  {"samples",samples}}}});
}

AnsiString FrameDebugger::TraceEntityPipelineJSON(const char *name_or_id)
{
    if(!name_or_id || !*name_or_id)
        return debug_bridge::DebugJsonToText(
            nlohmann::json{{"valid",false},{"error","missing entity name or ID"}});

    const PipelineInspectionSnapshot snap=GetPipelineInspection();
    if(!snap.valid)
        return debug_bridge::DebugJsonToText(
            nlohmann::json{{"valid",false},{"error","no pipeline inspection data available"}});

    uint32_t target_id=0;
    const bool is_numeric=(sscanf(name_or_id,"%u",&target_id)==1 && target_id>0);

    for(uint32_t i=0;i<snap.drop_sample_count;++i)
    {
        const auto &s=snap.drop_samples[i];
        if((is_numeric && s.entity_id==target_id) || (strcmp(s.entity_name,name_or_id)==0))
        {
            return debug_bridge::DebugJsonToText(nlohmann::json{
                {"valid",true},
                {"status","Dropped"},
                {"entity_name",s.entity_name},
                {"entity_id",s.entity_id},
                {"stage",s.stage},
                {"reason",s.reason},
                {"detail",s.detail}});
        }
    }

    return debug_bridge::DebugJsonToText(nlohmann::json{
        {"valid",true},
        {"status","RenderedOrPassed"},
        {"query",name_or_id},
        {"note","Entity not found in drop attribution samples. Likely passed all stages or fast-path replayed."}});
}
}//namespace hgl::graph
