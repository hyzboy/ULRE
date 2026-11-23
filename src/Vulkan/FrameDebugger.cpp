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
static constexpr uint32_t kPipelineSnapshotHistory=8;
FrameDebugger::PipelineInspectionSnapshot *PipelineInspectionHistory()
{
    static FrameDebugger::PipelineInspectionSnapshot snapshots[kPipelineSnapshotHistory];
    return snapshots;
}
uint32_t &PipelineInspectionHead(){ static uint32_t head=0; return head; }
uint32_t &PipelineInspectionCount(){ static uint32_t count=0; return count; }
uint64_t &PipelineInspectionSequence(){ static uint64_t sequence=0; return sequence; }
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
    PipelineInspectionSnapshot published=snapshot;
    if(published.drop_sample_count>PipelineInspectionSnapshot::kMaxSampleDrops)
        published.drop_sample_count=PipelineInspectionSnapshot::kMaxSampleDrops;
    if(published.trace_sample_count>PipelineInspectionSnapshot::kMaxTraceSamples)
        published.trace_sample_count=PipelineInspectionSnapshot::kMaxTraceSamples;
    published.schema_version=2;
    published.snapshot_sequence=++PipelineInspectionSequence();
    published.published_at_ms=hgl::GetUptimeMs();
    PipelineInspectionHistory()[PipelineInspectionHead()]=published;
    PipelineInspectionHead()=(PipelineInspectionHead()+1)%kPipelineSnapshotHistory;
    if(PipelineInspectionCount()<kPipelineSnapshotHistory)
        ++PipelineInspectionCount();
}

FrameDebugger::PipelineInspectionSnapshot FrameDebugger::GetPipelineInspection()
{
    hgl::MutexLock lock(&PipelineInspectionMutex());
    if(!PipelineInspectionCount())
        return PipelineInspectionSnapshot{};
    const uint32_t latest=(PipelineInspectionHead()+kPipelineSnapshotHistory-1)
                          %kPipelineSnapshotHistory;
    return PipelineInspectionHistory()[latest];
}

bool FrameDebugger::GetPipelineInspection(const uint64_t world_id,const uint32_t frame_serial,
                                          PipelineInspectionSnapshot &snapshot)
{
    hgl::MutexLock lock(&PipelineInspectionMutex());
    const uint32_t count=PipelineInspectionCount();
    for(uint32_t offset=0;offset<count;++offset)
    {
        const uint32_t index=(PipelineInspectionHead()+kPipelineSnapshotHistory-1-offset)
                             %kPipelineSnapshotHistory;
        const PipelineInspectionSnapshot &candidate=PipelineInspectionHistory()[index];
        if((!world_id||candidate.world_id==world_id)
           &&(!frame_serial||candidate.frame_serial==frame_serial))
        {
            snapshot=candidate;
            return true;
        }
    }
    return false;
}

AnsiString FrameDebugger::GetPipelineInspectionJSON(const uint64_t world_id,
                                                    const uint32_t frame_serial)
{
    PipelineInspectionSnapshot snap;
    if(!GetPipelineInspection(world_id,frame_serial,snap) || !snap.valid)
        return debug_bridge::DebugJsonToText(nlohmann::json{
            {"valid",false},{"error","pipeline_snapshot_not_found"}});

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
    nlohmann::json trace_samples=nlohmann::json::array();
    for(uint32_t i=0;i<snap.trace_sample_count;++i)
    {
        const auto &s=snap.trace_samples[i];
        trace_samples.push_back({
            {"name",s.entity_name},
            {"entity_id",s.entity_id},
            {"generation",s.generation},
            {"collect_status",s.collect_status},
            {"collect_reason",s.collect_reason},
            {"resource_observations",{{"geometry_pending",s.pending_geometry},
                                      {"material_pending",s.pending_material}}},
            {"batch_status",s.batch_status},
            {"batch_index",s.batch_index==UINT32_MAX?nlohmann::json():nlohmann::json(s.batch_index)},
            {"batch_reason",s.batch_reason}});
    }

    nlohmann::json gpu_draw_count=nlohmann::json();
    if(snap.gpu_draw_count_available)
        gpu_draw_count=snap.gpu_draw_count;
    nlohmann::json funnel={
        {"scene_primitives",snap.scene_primitives},
        {"passed_can_render",snap.passed_can_render},
        {"collect_candidates",snap.collect_candidates},
        {"collected_items",snap.collected_items},
        {"batched_items",snap.batched_items},
        {"batch_count",snap.batch_count},
        {"gpu_draw_count",gpu_draw_count},
        {"gpu_draw_count_available",snap.gpu_draw_count_available}};

    const uint64_t now=hgl::GetUptimeMs();
    const uint64_t age_ms=now>=snap.published_at_ms?now-snap.published_at_ms:0;
    const nlohmann::json collect_reason=snap.collect_complete?nlohmann::json():
        nlohmann::json(snap.fast_path_engaged?"collect_fast_path_replay":"collect_stage_unavailable");
    const nlohmann::json batch_reason=snap.batch_available?nlohmann::json():
        nlohmann::json("batch_stage_unavailable");
    const nlohmann::json gpu_cull_reason=snap.gpu_cull_available?nlohmann::json():
        nlohmann::json("frame_correlated_gpu_cull_unavailable");
    const nlohmann::json gpu_execution_reason=snap.gpu_execution_available?nlohmann::json():
        nlohmann::json("entity_level_gpu_execution_unavailable");
    const bool entity_trace_complete=snap.collect_complete
        &&!snap.trace_sample_truncated
        &&snap.trace_sample_count==snap.scene_primitives;
    const nlohmann::json entity_trace_reason=entity_trace_complete?nlohmann::json():
        nlohmann::json(snap.fast_path_engaged?"collect_fast_path_replay":
                       (snap.trace_sample_truncated?"trace_samples_truncated":"trace_coverage_incomplete"));
    return debug_bridge::DebugJsonToText(nlohmann::json{
        {"valid",true},
        {"schema_version",snap.schema_version},
        {"world_id",snap.world_id},
        {"frame_serial",snap.frame_serial},
        {"snapshot_sequence",snap.snapshot_sequence},
        {"snapshot_age_ms",age_ms},
        {"coverage",{{"collect",{{"available",snap.collect_available},
                                  {"complete",snap.collect_complete},
                                  {"reason",collect_reason}}},
                     {"batch",{{"available",snap.batch_available},
                                {"complete",snap.batch_available},
                                {"reason",batch_reason}}},
                     {"gpu_cull",{{"available",snap.gpu_cull_available},
                                   {"complete",snap.gpu_cull_available},
                                   {"reason",gpu_cull_reason}}},
                     {"gpu_execution",{{"available",snap.gpu_execution_available},
                                        {"complete",snap.gpu_execution_available},
                                        {"reason",gpu_execution_reason}}}}},
        {"funnel",funnel},
        {"funnel_units",{{"scene_primitives","primitive"},
                          {"passed_can_render","primitive"},
                          {"collect_candidates","primitive"},
                          {"collected_items","render_item"},
                          {"batched_items","render_item"},
                          {"batch_count","batch"},
                          {"gpu_draw_count","unavailable_until_frame_correlated"}}},
        {"funnel_definitions",{{"scene_primitives","primitive_components_in_main_frame_source_list"},
                                {"passed_can_render","primitive_components_with_renderable_fact"},
                                {"collect_candidates","primitive_components_eligible_for_current_pass"},
                                {"collected_items","actual_render_items_in_frame_cache"},
                                {"batched_items","items_in_nonempty_cpu_material_batches"},
                                {"batch_count","nonempty_cpu_material_batches"}}},
        {"funnel_provenance",{{"collect",snap.fast_path_engaged?"cache_replay":"current_frame"},
                               {"batch","current_frame_cpu_batch_membership"},
                               {"gpu_cull","unavailable"},
                               {"gpu_execution","unavailable"}}},
        {"cache_state",{{"fast_path_engaged",snap.fast_path_engaged},
                        {"last_collect_steady",snap.last_collect_steady},
                        {"has_pending_resources",snap.has_pending_resources},
                        {"snapshot_ready",snap.snapshot_ready},
                        {"snapshot_topology_rev",snap.snapshot_topology_rev},
                        {"world_topology_rev",snap.world_topology_rev},
                        {"snapshot_item_count",snap.snapshot_item_count}}},
        {"drops",{{"invisible",snap.dropped_invisible},
                  {"no_owner",snap.dropped_no_owner},
                  {"no_transform",snap.dropped_no_transform},
                  {"mobility_filtered",snap.dropped_mobility},
                  {"other",snap.dropped_other},
                  {"sample_truncated",snap.drop_sample_truncated},
                  {"samples",samples}}},
        {"entity_trace",{{"complete",entity_trace_complete},
                          {"reason",entity_trace_reason},
                          {"sample_count",snap.trace_sample_count},
                          {"sample_truncated",snap.trace_sample_truncated},
                          {"samples",trace_samples}}},
        {"resource_observations",{{"pending_material_resources",snap.pending_material_resources},
                                   {"pending_geometry_resources",snap.pending_geometry_resources},
                                   {"unit","primitive_observations"}}}});
}

AnsiString FrameDebugger::TraceEntityPipelineJSON(const char *name_or_id,
                                                   const uint64_t world_id,
                                                   const uint32_t frame_serial)
{
    if(!name_or_id || !*name_or_id)
        return debug_bridge::DebugJsonToText(
            nlohmann::json{{"valid",false},{"error","missing entity name or ID"}});

    PipelineInspectionSnapshot snap;
    if(!GetPipelineInspection(world_id,frame_serial,snap)||!snap.valid)
        return debug_bridge::DebugJsonToText(
            nlohmann::json{{"valid",false},{"error","no pipeline inspection data available"}});

    uint32_t target_id=0;
    bool is_numeric=true;
    bool numeric_overflow=false;
    for(const unsigned char *p=reinterpret_cast<const unsigned char *>(name_or_id);*p;++p)
    {
        if(*p<'0'||*p>'9')
        {
            is_numeric=false;
            break;
        }
        const uint32_t digit=*p-'0';
        if(target_id>(UINT32_MAX-digit)/10)
        {
            numeric_overflow=true;
            break;
        }
        target_id=target_id*10+digit;
    }
    if(is_numeric && (numeric_overflow||target_id==0))
        return debug_bridge::DebugJsonToText(nlohmann::json{
            {"valid",false},{"error","invalid_entity_id"}});

    const PipelineInspectionSnapshot::TraceSample *match=nullptr;
    for(uint32_t i=0;i<snap.trace_sample_count;++i)
    {
        const auto &sample=snap.trace_samples[i];
        if((is_numeric&&sample.entity_id==target_id)
           ||(!is_numeric&&::strcmp(sample.entity_name,name_or_id)==0))
        {
            if(match&&(match->entity_id!=sample.entity_id
                       ||match->generation!=sample.generation))
                return debug_bridge::DebugJsonToText(nlohmann::json{
                    {"valid",true},{"status","Ambiguous"},{"query",name_or_id},
                    {"reason","duplicate_entity_name_in_snapshot"}});
            match=&sample;
        }
    }

    if(match)
    {
        const char *status="Unknown";
        if(::strcmp(match->collect_status,"filtered")==0) status="Filtered";
        else if(::strcmp(match->collect_status,"pending")==0) status="Pending";
        else if(::strcmp(match->collect_status,"failed")==0) status="Failed";
        else if(::strcmp(match->collect_status,"candidate")==0) status="CollectCandidate";
        else if(::strcmp(match->collect_status,"collected")==0)
        {
            status="Collected";
            if(::strcmp(match->batch_status,"batched")==0) status="Batched";
            else if(::strcmp(match->batch_status,"filtered")==0) status="BatchFiltered";
        }

        return debug_bridge::DebugJsonToText(nlohmann::json{
            {"valid",true},
            {"status",status},
            {"world_id",snap.world_id},
            {"frame_serial",snap.frame_serial},
            {"entity_name",match->entity_name},
            {"entity_id",match->entity_id},
            {"generation",match->generation},
            {"collect",{{"status",match->collect_status},
                        {"reason",match->collect_reason}}},
            {"resource_observations",{{"geometry_pending",match->pending_geometry},
                                      {"material_pending",match->pending_material}}},
            {"batch",{{"status",match->batch_status},
                      {"index",match->batch_index==UINT32_MAX?nlohmann::json():nlohmann::json(match->batch_index)},
                      {"reason",match->batch_reason}}}});
    }

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

    nlohmann::json unknown={
        {"valid",true},
        {"status","Unknown"},
        {"world_id",snap.world_id},
        {"frame_serial",snap.frame_serial},
        {"query",name_or_id},
        {"reason",snap.trace_sample_truncated?"trace_samples_truncated":"entity_not_sampled"}};
    return debug_bridge::DebugJsonToText(unknown);
}
}//namespace hgl::graph
