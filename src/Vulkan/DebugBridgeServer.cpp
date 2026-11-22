#include<hgl/vk/DebugBridgeServer.h>
#include<hgl/vk/RenderHealthLedger.h>
#include<hgl/vk/FrameDebugger.h>
#include<hgl/vk/DebugCaptureQueue.h>
#include<hgl/vk/VKQueryProfiler.h>
#include<hgl/ecs/support/debug/DebugViewController.h>
#include<hgl/ecs/support/debug/DebugViewPass.h>
#include<hgl/mtl/DebugViewMode.h>
#include<hgl/common/DebugChannel.h>
#include<hgl/time/Time.h>
#include<hgl/log/Log.h>
#include "DebugBridgeProtocol.h"
#include<cstring>
#include<cinttypes>

namespace hgl::graph
{
#ifdef _WIN32
static AnsiString MakeMetricsJSON()
{
    GpuQueryProfiler *profiler=GpuQueryProfiler::GetActive();
    if(!profiler) return AnsiString("{\"valid\":false}");
    const GpuQueryProfiler::FrameMetric *frame=profiler->GetLatestFrame();
    if(!frame) return AnsiString("{\"valid\":false}");

    AnsiString json="{\"valid\":true,\"frame_id\":";
    char number[64];
    snprintf(number,sizeof(number),"%" PRIu64,static_cast<uint64_t>(frame->frame_id));
    json+=AnsiString(number);
    json+=",\"gpu_total_time_ms\":";
    snprintf(number,sizeof(number),"%.6f",frame->total_gpu_ms);
    json+=AnsiString(number);
    json+=",\"passes\":[";
    for(uint32_t i=0;i<frame->pass_count;i++)
    {
        const GpuQueryProfiler::PassMetric &pass=frame->passes[i];
        if(i) json+=",";
        json+="{\"name\":\"";
        json+=pass.name;
        json+="\",\"gpu_time_ms\":";
        snprintf(number,sizeof(number),"%.6f",pass.gpu_ms);
        json+=AnsiString(number);
        json+="}";
    }
    json+="]}";
    return json;
}

static AnsiString MakeDebugViewJSON()
{
    return hgl::ecs::DebugViewPass::GetStatusJSON();
}

static AnsiString SetDebugViewFromRequest(const char *mode_name)
{
    hgl::graph::mtl::DebugViewMode mode;

    if(!hgl::graph::mtl::ParseDebugViewMode(mode_name,mode))
        return AnsiString("{\"valid\":false,\"error\":\"unknown_debug_view_mode\"}");

    hgl::ecs::DebugViewController::RequestMode(mode);
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

static bool SendPipeResponse(HANDLE pipe,HANDLE stop_event,const char *id,
                             const AnsiString &body,bool error=false)
{
    AnsiString response="{\"jsonrpc\":\"2.0\",\"id\":";
    response+=id;
    response+=error?",\"error\":" : ",\"result\":";
    response+=body;
    response+="}";

    DWORD offset=0;
    const DWORD length=static_cast<DWORD>(response.Length());
    while(offset<length)
    {
        DWORD written=0;
        if(!TransferPipe(pipe,stop_event,nullptr,response.c_str()+offset,
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
            if(SendPipeResponse(pipe_handle,stop_event,"null",
                                "{\"code\":-32600,\"message\":\"Request too large\"}",true))
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
    AnsiString result;
    if(!valid)
        result="{\"code\":-32600,\"message\":\"Invalid request\"}";
    else
    {
        const AnsiString &method=parsed.GetMethod();
        const AnsiString mode=parsed.GetParamString("mode");
        const AnsiString attachment=parsed.GetParamString("attachment");
        uint64_t target_id=0,request_id=0,resource_id=0,offset=0,size=0;
        const bool has_target_id=parsed.GetParamUInt64("target_id",target_id);
        const bool has_request_id=parsed.GetParamUInt64("request_id",request_id);
        const bool has_resource_id=parsed.GetParamUInt64("resource_id",resource_id);
        const bool has_offset=parsed.GetParamUInt64("offset",offset);
        const bool has_size=parsed.GetParamUInt64("size",size);

        if(method=="get_render_health")
            result=RenderHealthLedger::GetStatusJSON();
        else if(method=="get_pass_metrics")
            result=MakeMetricsJSON();
        else if(method=="get_camera_matrices")
            result=FrameDebugger::GetCameraSnapshotJSON();
        else if(method=="inspect_render_pipeline" || method=="get_pipeline_inspection")
            result=FrameDebugger::GetPipelineInspectionJSON();
        else if(method=="trace_entity_pipeline" || method=="trace_entity_render_state")
            result=FrameDebugger::TraceEntityPipelineJSON(parsed.GetEntity().c_str());
        else if(method=="set_debug_view")
            result=SetDebugViewFromRequest(mode.c_str());
        else if(method=="get_debug_view")
            result=MakeDebugViewJSON();
        else if(method=="set_offscreen_debug_view")
        {
            hgl::graph::mtl::DebugViewMode view_mode;
            if(!hgl::graph::mtl::ParseDebugViewMode(mode.c_str(),view_mode))
                result="{\"valid\":false,\"error\":\"unknown_debug_view_mode\"}";
            else
            {
                hgl::ecs::DebugViewPass::RequestMode(view_mode);
                result=hgl::ecs::DebugViewPass::GetStatusJSON();
            }
        }
        else if(method=="get_offscreen_debug_view")
            result=hgl::ecs::DebugViewPass::GetStatusJSON();
        else if(method=="list_capture_targets")
            result=DebugCaptureQueue::GetTargetsJSON();
        else if(method=="list_capture_buffers")
            result=DebugCaptureQueue::GetBuffersJSON();
        else if(method=="capture_buffer")
        {
            if(!has_resource_id || !has_offset || !has_size)
                result="{\"valid\":false,\"error\":\"missing_buffer_range\"}";
            else
            {
                const char *error=nullptr;
                const uint64_t capture_request_id=DebugCaptureQueue::RequestBuffer(
                    resource_id,offset,size,&error);
                if(capture_request_id)
                    result=DebugCaptureQueue::GetResultJSON(capture_request_id);
                else
                {
                    result="{\"valid\":false,\"error\":\"";
                    result+=error;
                    result+="\"}";
                }
            }
        }
        else if(method=="capture_frame")
        {
            if(!has_target_id)
                result="{\"valid\":false,\"error\":\"missing_target_id\"}";
            else if(attachment!="color" && attachment!="depth" && attachment!="color_depth")
                result="{\"valid\":false,\"error\":\"unsupported_attachment\"}";
            else
            {
                const auto attachment_type=attachment=="color"
                    ?DebugCaptureQueue::Attachment::Color
                    :(attachment=="depth"
                        ?DebugCaptureQueue::Attachment::Depth:DebugCaptureQueue::Attachment::ColorDepth);
                const char *error=nullptr;
                const uint64_t capture_request_id=DebugCaptureQueue::Request(target_id,attachment_type,&error);
                if(capture_request_id)
                    result=DebugCaptureQueue::GetResultJSON(capture_request_id);
                else
                {
                    result="{\"valid\":false,\"error\":\"";
                    result+=error;
                    result+="\"}";
                }
            }
        }
        else if(method=="get_capture_status")
            result=has_request_id?DebugCaptureQueue::GetResultJSON(request_id)
                                 :AnsiString("{\"valid\":false,\"error\":\"missing_request_id\"}");
        else if(method=="release_capture")
            result=has_request_id && DebugCaptureQueue::Release(request_id)
                  ?AnsiString("{\"valid\":true}")
                  :AnsiString("{\"valid\":false,\"error\":\"request_not_terminal_or_unknown\"}");
        else if(method=="cancel_capture")
            result=has_request_id && DebugCaptureQueue::Cancel(request_id)
                  ?AnsiString("{\"valid\":true}")
                  :AnsiString("{\"valid\":false,\"error\":\"request_not_queued_or_unknown\"}");
        else if(method=="dump_buffer" || method=="dump_render_target")
            result=FrameDebugger::GetLatestResultJSON();
        else if(method=="inspect_camera_viewport")
            result=RenderHealthLedger::GetStatusJSON();
        else
        {
            protocol_error=true;
            result="{\"code\":-32601,\"message\":\"Method not found\"}";
        }
    }

    AnsiString id_token("null");
    if(valid)
    {
        U8String id_text;
        OSString id_error;
        if(JsonToString(parsed.GetId(),id_text,id_error))
            id_token=AnsiString(reinterpret_cast<const char*>(id_text.c_str()),id_text.Length());
    }

    if(SendPipeResponse(pipe_handle,stop_event,id_token.c_str(),result,protocol_error))
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
