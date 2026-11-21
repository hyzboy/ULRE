#include<hgl/vk/DebugBridgeServer.h>
#include<hgl/vk/RenderHealthLedger.h>
#include<hgl/vk/FrameDebugger.h>
#include<hgl/vk/VKQueryProfiler.h>
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

static void SendPipeResponse(HANDLE pipe,const AnsiString &body)
{
    AnsiString response="{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":";
    response+=body;
    response+="}";

    DWORD written=0;
    WriteFile(pipe,response.c_str(),static_cast<DWORD>(response.Length()),&written,nullptr);
}
#endif

bool DebugBridgeServer::ProcStartThread()
{
#ifdef _WIN32
    started=true;
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
    started=false;
#endif
}

void DebugBridgeServer::OnStopRequested()
{
#ifdef _WIN32
    if(pipe_handle!=INVALID_HANDLE_VALUE)
        CancelIoEx(pipe_handle,nullptr);
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
    if(started || IsRunning()) return false;
    return Start();
#else
    return false;
#endif
}

void DebugBridgeServer::StopServer()
{
#ifdef _WIN32
    if(IsRunning())
        StopAndJoin(2.0);
#endif
}

bool DebugBridgeServer::StartActive()
{
    static DebugBridgeServer server;
    return server.StartServer();
}

void DebugBridgeServer::StopActive()
{
    static DebugBridgeServer server;
    server.StopServer();
}

bool DebugBridgeServer::Execute()
{
#ifdef _WIN32
    pipe_handle=CreateNamedPipeA(
        "\\\\.\\pipe\\ulre_render_debug",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT,
        1,4096,4096,0,nullptr);

    if(pipe_handle==INVALID_HANDLE_VALUE)
    {
        GLogError(u8"[DebugBridge] CreateNamedPipe failed: %u",static_cast<unsigned>(GetLastError()));
        return false;
    }

    if(IsStopRequested())
        return false;

    const BOOL connected=ConnectNamedPipe(pipe_handle,nullptr)
                       || GetLastError()==ERROR_PIPE_CONNECTED;
    if(!connected)
        return IsStopRequested()?false:true;

    char request[4096]={};
    DWORD bytes_read=0;
    if(!ReadFile(pipe_handle,request,sizeof(request)-1,&bytes_read,nullptr))
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
    else if(ContainsMethod(request,"dump_buffer"))
        result=FrameDebugger::GetLatestResultJSON();
    else
        result="{\"valid\":false,\"error\":\"unknown_method\"}";

    SendPipeResponse(pipe_handle,result);
    FlushFileBuffers(pipe_handle);
    DisconnectNamedPipe(pipe_handle);
    CloseHandle(pipe_handle);
    pipe_handle=INVALID_HANDLE_VALUE;
    return !IsStopRequested();
#else
    return false;
#endif
}
}//namespace hgl::graph
