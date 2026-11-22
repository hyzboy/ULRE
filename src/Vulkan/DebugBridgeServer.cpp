#include<hgl/vk/DebugBridgeServer.h>
#include<hgl/vk/RenderHealthLedger.h>
#include<hgl/vk/FrameDebugger.h>
#include<hgl/vk/VKQueryProfiler.h>
#include<hgl/ecs/support/debug/DebugViewController.h>
#include<hgl/mtl/DebugViewMode.h>
#include<hgl/common/DebugChannel.h>
#include<hgl/log/Log.h>
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

static bool ContainsMethod(const char *request,const char *method)
{
    return request && method && ::strstr(request,method)!=nullptr;
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

static AnsiString SetDebugViewFromRequest(const char *request)
{
    hgl::graph::mtl::DebugViewMode mode;

    if(!request)
        return AnsiString("{\"valid\":false,\"error\":\"missing_request\"}");

    const char *mode_name=nullptr;
    if(::strstr(request,"world_normal")) mode_name="world_normal";
    else if(::strstr(request,"base_color")) mode_name="base_color";
    else if(::strstr(request,"surface")) mode_name="surface";
    else if(::strstr(request,"emissive")) mode_name="emissive";
    else if(::strstr(request,"uv0")) mode_name="uv0";
    else if(::strstr(request,"off")) mode_name="off";
    else
        return AnsiString("{\"valid\":false,\"error\":\"unknown_debug_view_mode\"}");

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
                         DWORD size,DWORD &transferred,DWORD timeout)
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
    CloseHandle(operation.hEvent);
    return success;
}

static bool SendPipeResponse(HANDLE pipe,HANDLE stop_event,const AnsiString &body)
{
    AnsiString response="{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":";
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
        PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,
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
    if(!TransferPipe(pipe_handle,stop_event,request,nullptr,sizeof(request)-1,
                     bytes_read,5000) || bytes_read==0)
    {
        DisconnectNamedPipe(pipe_handle);
        CloseHandle(pipe_handle);
        pipe_handle=INVALID_HANDLE_VALUE;
        return !IsStopRequested();
    }
    request[bytes_read]=0;

    AnsiString result;
    if(ContainsMethod(request,"get_render_health"))
        result=RenderHealthLedger::GetStatusJSON();
    else if(ContainsMethod(request,"get_pass_metrics"))
        result=MakeMetricsJSON();
    else if(ContainsMethod(request,"get_camera_matrices"))
        result=FrameDebugger::GetCameraSnapshotJSON();
    else if(ContainsMethod(request,"set_debug_view"))
        result=SetDebugViewFromRequest(request);
    else if(ContainsMethod(request,"get_debug_view"))
        result=MakeDebugViewJSON();
    else if(ContainsMethod(request,"capture_frame"))
        result="{\"valid\":false,\"error\":\"capture_request_not_supported\"}";
    else if(ContainsMethod(request,"dump_buffer")
         || ContainsMethod(request,"dump_render_target"))
        result=FrameDebugger::GetLatestResultJSON();
    else if(ContainsMethod(request,"inspect_camera_viewport"))
        result=RenderHealthLedger::GetStatusJSON();
    else
        result="{\"valid\":false,\"error\":\"unknown_method\"}";

    if(SendPipeResponse(pipe_handle,stop_event,result))
    {
        char ignored;
        DWORD received=0;
        TransferPipe(pipe_handle,stop_event,&ignored,nullptr,1,received,5000);
    }
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
