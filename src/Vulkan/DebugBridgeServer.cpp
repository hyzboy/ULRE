#include<hgl/vk/DebugBridgeServer.h>
#include<hgl/vk/RenderHealthLedger.h>
#include<hgl/vk/FrameDebugger.h>
#include<hgl/vk/VKQueryProfiler.h>
#include<hgl/ecs/support/debug/DebugViewController.h>
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
    const hgl::graph::mtl::DebugViewMode mode=
        hgl::ecs::DebugViewController::GetAppliedMode();

    AnsiString json="{\"valid\":true,\"mode\":\"";
    json+=hgl::graph::mtl::GetDebugViewModeName(mode);
    json+="\",\"requested_mode\":\"";
    json+=hgl::graph::mtl::GetDebugViewModeName(
        hgl::ecs::DebugViewController::GetRequestedMode());
    json+="\",\"tracked_entities\":";
    json+=AnsiString::numberOf(hgl::ecs::DebugViewController::GetTrackedCount());
    json+="}";
    return json;
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
                         static_cast<DWORD>(deadline-now),&error)
           && error!=ERROR_MORE_DATA)
            return;
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
    debug_bridge::Parser parser(request,bytes_read);
    const bool valid=parser.Parse(parsed);
    bool protocol_error=!valid;
    AnsiString result;
    if(!valid)
        result="{\"code\":-32600,\"message\":\"Invalid request\"}";
    else if(::strcmp(parsed.method,"get_render_health")==0)
        result=RenderHealthLedger::GetStatusJSON();
    else if(::strcmp(parsed.method,"get_pass_metrics")==0)
        result=MakeMetricsJSON();
    else if(::strcmp(parsed.method,"get_camera_matrices")==0)
        result=FrameDebugger::GetCameraSnapshotJSON();
    else if(::strcmp(parsed.method,"set_debug_view")==0)
        result=SetDebugViewFromRequest(parsed.mode);
    else if(::strcmp(parsed.method,"get_debug_view")==0)
        result=MakeDebugViewJSON();
    else if(::strcmp(parsed.method,"capture_frame")==0)
        result="{\"valid\":false,\"error\":\"capture_request_not_supported\"}";
    else if(::strcmp(parsed.method,"dump_buffer")==0
         || ::strcmp(parsed.method,"dump_render_target")==0)
        result=FrameDebugger::GetLatestResultJSON();
    else if(::strcmp(parsed.method,"inspect_camera_viewport")==0)
        result=RenderHealthLedger::GetStatusJSON();
    else
    {
        protocol_error=true;
        result="{\"code\":-32601,\"message\":\"Method not found\"}";
    }

    if(SendPipeResponse(pipe_handle,stop_event,valid?parsed.id:"null",result,protocol_error))
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
