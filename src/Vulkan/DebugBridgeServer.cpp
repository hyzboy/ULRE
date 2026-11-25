#include<hgl/vk/DebugBridgeServer.h>
#include<hgl/vk/RenderHealthLedger.h>
#include<hgl/vk/FrameDebugger.h>
#include<hgl/vk/DebugCaptureQueue.h>
#include<hgl/vk/VKQueryProfiler.h>
#include<hgl/vk/DebugViewState.h>
#include<hgl/mtl/DebugViewMode.h>
#include<hgl/common/DebugChannel.h>
#include<hgl/time/Time.h>
#include<hgl/log/Log.h>
#include "DebugBridgeProtocol.h"
#include "DebugBridgeJson.h"

namespace hgl::graph
{
#ifdef _WIN32
static nlohmann::json MakeMetricsJSON()
{
    GpuQueryProfiler *profiler=GpuQueryProfiler::GetActive();
    if(!profiler) return nlohmann::json{{"valid",false}};
    GpuQueryProfiler::FrameMetric frame;
    if(!profiler->GetLatestFrameSnapshot(frame))
        return nlohmann::json{{"valid",false}};

    nlohmann::json passes=nlohmann::json::array();
    for(uint32_t i=0;i<frame.pass_count;i++)
    {
        const GpuQueryProfiler::PassMetric &pass=frame.passes[i];
        passes.push_back({{"name",pass.name.c_str()},{"gpu_time_ms",pass.gpu_ms}});
    }
    return nlohmann::json{
        {"valid",true},
        {"frame_id",frame.frame_id},
        {"gpu_total_time_ms",frame.total_gpu_ms},
        {"passes",passes}};
}

static nlohmann::json MakeDebugViewJSON()
{
    return debug_bridge::DebugJsonFromText(DebugViewState::GetStatusJSON());
}

static nlohmann::json MakeCameraViewportJSON()
{
    return nlohmann::json{
        {"valid",false},
        {"status","unavailable"},
        {"error","camera_viewport_contract_not_sampled"}};
}

static nlohmann::json SetDebugViewFromRequest(const char *mode_name)
{
    hgl::graph::mtl::DebugViewMode mode;

    if(!hgl::graph::mtl::ParseDebugViewMode(mode_name,mode))
        return nlohmann::json{{"valid",false},{"error","unknown_debug_view_mode"}};

    DebugViewState::RequestMode(mode);
    return MakeDebugViewJSON();
}

static bool WaitPipeOperation(HANDLE pipe,HANDLE stop_event,OVERLAPPED &operation,
                              DWORD &transferred,DWORD timeout=INFINITE)
{
    HANDLE events[]={stop_event,operation.hEvent};
    const DWORD status=WaitForMultipleObjects(2,events,FALSE,timeout);
    if(status==WAIT_OBJECT_0+1)
        return GetOverlappedResult(pipe,&operation,&transferred,FALSE)!=FALSE;

    CancelIoEx(pipe,&operation);
    GetOverlappedResult(pipe,&operation,&transferred,TRUE);
    return false;
}

static bool ConnectPipe(HANDLE pipe,HANDLE stop_event)
{
    OVERLAPPED operation{};
    operation.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!operation.hEvent)
        return false;

    bool connected=ConnectNamedPipe(pipe,&operation)!=FALSE;
    if(!connected)
    {
        const DWORD error=GetLastError();
        if(error==ERROR_PIPE_CONNECTED)
            connected=true;
        else if(error==ERROR_IO_PENDING)
        {
            DWORD ignored=0;
            connected=WaitPipeOperation(pipe,stop_event,operation,ignored);
        }
    }
    CloseHandle(operation.hEvent);
    return connected;
}

static bool TransferPipe(HANDLE pipe,HANDLE stop_event,void *read_buffer,const void *write_buffer,
                         DWORD size,DWORD &transferred,DWORD timeout,DWORD *failure=nullptr)
{
    OVERLAPPED operation{};
    operation.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!operation.hEvent)
        return false;

    transferred=0;
    const BOOL done=write_buffer?WriteFile(pipe,write_buffer,size,&transferred,&operation)
                                :ReadFile(pipe,read_buffer,size,&transferred,&operation);
    bool success=done!=FALSE;
    if(!success && GetLastError()==ERROR_IO_PENDING)
        success=WaitPipeOperation(pipe,stop_event,operation,transferred,timeout);
    if(!success && failure)
        *failure=GetLastError();
    CloseHandle(operation.hEvent);
    return success;
}

static bool SendPipeResponse(HANDLE pipe,HANDLE stop_event,const nlohmann::json &id,
                             const nlohmann::json &body,bool error=false)
{
    nlohmann::json response={{"jsonrpc","2.0"},{"id",id}};
    response[error?"error":"result"]=body;

    const AnsiString text=debug_bridge::DebugJsonToText(response);
    if(text.IsEmpty())
        return false;

    DWORD offset=0;
    const DWORD length=static_cast<DWORD>(text.Length());
    while(offset<length)
    {
        DWORD written=0;
        if(!TransferPipe(pipe,stop_event,nullptr,text.c_str()+offset,
                         length-offset,written,5000) || written==0)
            return false;
        offset+=written;
    }
    return true;
}

static void WaitForClientClose(HANDLE pipe,HANDLE stop_event)
{
    const uint64_t deadline=hgl::GetUptimeMs()+5000;
    char ignored[256];
    for(;;)
    {
        const uint64_t now=hgl::GetUptimeMs();
        if(now>=deadline) return;
        DWORD received=0;
        DWORD error=0;
        if(!TransferPipe(pipe,stop_event,ignored,nullptr,sizeof(ignored),received,
                         static_cast<DWORD>(deadline-now),&error))
        {
            if(error==ERROR_MORE_DATA) continue;
            return;
        }
        if(received==0) return;
    }
}

#endif

static DebugBridgeServer &ActiveBridge()
{
    static DebugBridgeServer server;
    return server;
}

DebugBridgeServer::DebugBridgeServer(const char *name):pipe_name(name)
{
}

bool DebugBridgeServer::ProcStartThread()
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

void DebugBridgeServer::ProcEndThread()
{
#ifdef _WIN32
    if(pipe_handle!=INVALID_HANDLE_VALUE)
    {
        CloseHandle(pipe_handle);
        pipe_handle=INVALID_HANDLE_VALUE;
    }
#endif
}

void DebugBridgeServer::OnStopRequested()
{
#ifdef _WIN32
    if(stop_event)
        SetEvent(stop_event);
#endif
}

DebugBridgeServer::~DebugBridgeServer()
{
    StopServer();
}

bool DebugBridgeServer::StartServer()
{
#ifdef _WIN32
    if(!ULRE_DEBUG_ON(DebugBridge))
        return false;
    if(IsRunning()) return false;
    if(stop_event)
    {
        if(!Join()) return false;
        CloseHandle(stop_event);
        stop_event=nullptr;
    }
    stop_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    if(!stop_event)
    {
        GLogError(u8"[DebugBridge] CreateEvent failed: %u",static_cast<unsigned>(GetLastError()));
        return false;
    }
    if(Start()) return true;
    CloseHandle(stop_event);
    stop_event=nullptr;
    return false;
#else
    return false;
#endif
}

void DebugBridgeServer::StopServer()
{
#ifdef _WIN32
    if(stop_event)
    {
        if(!StopAndJoin())
        {
            GLogError(u8"[DebugBridge] Failed to stop server thread");
            return;
        }
        CloseHandle(stop_event);
        stop_event=nullptr;
    }
#endif
}

bool DebugBridgeServer::StartActive()
{
    return ActiveBridge().StartServer();
}

void DebugBridgeServer::StopActive()
{
    ActiveBridge().StopServer();
}

bool DebugBridgeServer::Execute()
{
#ifdef _WIN32
    pipe_handle=CreateNamedPipeA(
        pipe_name.c_str(),
        PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT,
        1,4096,4096,0,nullptr);

    if(pipe_handle==INVALID_HANDLE_VALUE)
    {
        GLogError(u8"[DebugBridge] CreateNamedPipe failed: %u",static_cast<unsigned>(GetLastError()));
        return false;
    }

    if(IsStopRequested())
        return false;

    if(!ConnectPipe(pipe_handle,stop_event))
    {
        CloseHandle(pipe_handle);
        pipe_handle=INVALID_HANDLE_VALUE;
        return !IsStopRequested();
    }

    char request[4096]={};
    DWORD bytes_read=0;
    DWORD read_error=0;
    if(!TransferPipe(pipe_handle,stop_event,request,nullptr,sizeof(request)-1,
                     bytes_read,5000,&read_error) || bytes_read==0)
    {
        if(read_error==ERROR_MORE_DATA)
        {
            if(SendPipeResponse(pipe_handle,stop_event,nlohmann::json(),
                                nlohmann::json{{"code",-32600},{"message","Request too large"}},true))
                WaitForClientClose(pipe_handle,stop_event);
        }
        DisconnectNamedPipe(pipe_handle);
        CloseHandle(pipe_handle);
        pipe_handle=INVALID_HANDLE_VALUE;
        return !IsStopRequested();
    }
    request[bytes_read]=0;

    debug_bridge::Request parsed;
    const bool valid=parsed.Parse(request,bytes_read);
    bool protocol_error=!valid;
    nlohmann::json result;
    if(!valid)
        result=nlohmann::json{{"code",-32600},{"message","Invalid request"}};
    else
    {
        const AnsiString &method=parsed.GetMethod();
        const AnsiString mode=parsed.GetParamString("mode");
        const AnsiString attachment=parsed.GetParamString("attachment");
        uint64_t target_id=0,request_id=0,resource_id=0,offset=0,size=0;
        uint64_t query_world_id=0,query_frame_serial=0;
        const bool has_target_id=parsed.GetParamUInt64("target_id",target_id);
        const bool has_request_id=parsed.GetParamUInt64("request_id",request_id);
        const bool has_resource_id=parsed.GetParamUInt64("resource_id",resource_id);
        const bool has_offset=parsed.GetParamUInt64("offset",offset);
        const bool has_size=parsed.GetParamUInt64("size",size);
        const bool has_query_world_id=parsed.GetParamUInt64("world_id",query_world_id);
        const bool has_query_frame_serial=parsed.GetParamUInt64("frame_serial",query_frame_serial);
        const bool invalid_query_selector=
            (parsed.HasParam("world_id")&&!has_query_world_id)
            ||(parsed.HasParam("frame_serial")
               &&(!has_query_frame_serial||query_frame_serial>UINT32_MAX));

        if(method=="get_render_health")
            result=debug_bridge::DebugJsonFromText(RenderHealthLedger::GetStatusJSON());
        else if(method=="get_pass_metrics")
            result=MakeMetricsJSON();
        else if(method=="get_camera_matrices")
            result=debug_bridge::DebugJsonFromText(FrameDebugger::GetCameraSnapshotJSON());
        else if(method=="inspect_render_pipeline" || method=="get_pipeline_inspection")
            result=invalid_query_selector
                ?nlohmann::json{{"valid",false},{"error","invalid_pipeline_snapshot_selector"}}
                :debug_bridge::DebugJsonFromText(FrameDebugger::GetPipelineInspectionJSON(
                    has_query_world_id?query_world_id:0,
                    has_query_frame_serial?static_cast<uint32_t>(query_frame_serial):0));
        else if(method=="trace_entity_pipeline" || method=="trace_entity_render_state")
            result=invalid_query_selector
                ?nlohmann::json{{"valid",false},{"error","invalid_pipeline_snapshot_selector"}}
                :debug_bridge::DebugJsonFromText(FrameDebugger::TraceEntityPipelineJSON(
                    parsed.GetEntity().c_str(),has_query_world_id?query_world_id:0,
                    has_query_frame_serial?static_cast<uint32_t>(query_frame_serial):0));
        else if(method=="set_debug_view")
            result=SetDebugViewFromRequest(mode.c_str());
        else if(method=="get_debug_view")
            result=MakeDebugViewJSON();
        else if(method=="set_offscreen_debug_view")
        {
            hgl::graph::mtl::DebugViewMode view_mode;
            if(!hgl::graph::mtl::ParseDebugViewMode(mode.c_str(),view_mode))
                result=nlohmann::json{{"valid",false},{"error","unknown_debug_view_mode"}};
            else
            {
                DebugViewState::RequestMode(view_mode);
                result=MakeDebugViewJSON();
            }
        }
        else if(method=="get_offscreen_debug_view")
            result=MakeDebugViewJSON();
        else if(method=="list_capture_targets")
            result=debug_bridge::DebugJsonFromText(DebugCaptureQueue::GetTargetsJSON());
        else if(method=="list_capture_buffers")
            result=debug_bridge::DebugJsonFromText(DebugCaptureQueue::GetBuffersJSON());
        else if(method=="capture_buffer")
        {
            if(!has_resource_id || !has_offset || !has_size)
                result=nlohmann::json{{"valid",false},{"error","missing_buffer_range"}};
            else
            {
                const char *error=nullptr;
                const uint64_t capture_request_id=DebugCaptureQueue::RequestBuffer(
                    resource_id,offset,size,&error);
                if(capture_request_id)
                    result=debug_bridge::DebugJsonFromText(DebugCaptureQueue::GetResultJSON(capture_request_id));
                else
                    result=nlohmann::json{{"valid",false},{"error",error}};
            }
        }
        else if(method=="capture_frame")
        {
            if(!has_target_id)
                result=nlohmann::json{{"valid",false},{"error","missing_target_id"}};
            else if(attachment!="color" && attachment!="depth" && attachment!="color_depth")
                result=nlohmann::json{{"valid",false},{"error","unsupported_attachment"}};
            else
            {
                const auto attachment_type=attachment=="color"
                    ?DebugCaptureQueue::Attachment::Color
                    :(attachment=="depth"
                        ?DebugCaptureQueue::Attachment::Depth:DebugCaptureQueue::Attachment::ColorDepth);
                const char *error=nullptr;
                const uint64_t capture_request_id=DebugCaptureQueue::Request(target_id,attachment_type,&error);
                if(capture_request_id)
                    result=debug_bridge::DebugJsonFromText(DebugCaptureQueue::GetResultJSON(capture_request_id));
                else
                    result=nlohmann::json{{"valid",false},{"error",error}};
            }
        }
        else if(method=="get_capture_status")
            result=has_request_id?debug_bridge::DebugJsonFromText(DebugCaptureQueue::GetResultJSON(request_id))
                                 :nlohmann::json{{"valid",false},{"error","missing_request_id"}};
        else if(method=="release_capture")
            result=has_request_id && DebugCaptureQueue::Release(request_id)
                  ?nlohmann::json{{"valid",true}}
                  :nlohmann::json{{"valid",false},{"error","request_not_terminal_or_unknown"}};
        else if(method=="cancel_capture")
            result=has_request_id && DebugCaptureQueue::Cancel(request_id)
                  ?nlohmann::json{{"valid",true}}
                  :nlohmann::json{{"valid",false},{"error","request_not_queued_or_unknown"}};
        else if(method=="dump_buffer" || method=="dump_render_target")
            result=nlohmann::json{
                {"valid",false},
                {"error","deprecated_dump_api"},
                {"reason","Use list_capture_targets/list_capture_buffers plus capture_frame/capture_buffer"}};
        else if(method=="inspect_camera_viewport")
            result=MakeCameraViewportJSON();
        else
        {
            protocol_error=true;
            result=nlohmann::json{{"code",-32601},{"message","Method not found"}};
        }
    }

    if(SendPipeResponse(pipe_handle,stop_event,valid?parsed.GetId():nlohmann::json(),result,protocol_error))
        WaitForClientClose(pipe_handle,stop_event);
    else if(!IsStopRequested())
        GLogError(u8"[DebugBridge] Failed to send pipe response");
    DisconnectNamedPipe(pipe_handle);
    CloseHandle(pipe_handle);
    pipe_handle=INVALID_HANDLE_VALUE;
    return !IsStopRequested();
#else
    return false;
#endif
}
}//namespace hgl::graph
