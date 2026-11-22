#include<hgl/vk/DebugBridgeServer.h>
#include<hgl/common/DebugChannel.h>
#include<hgl/time/Time.h>
#include<hgl/log/Log.h>
#include<cstring>
#include "../../Vulkan/DebugBridgeProtocol.h"

#ifdef _WIN32
namespace
{
    HANDLE Connect(const hgl::AnsiString &name)
    {
        for(int i=0;i<200;++i)
        {
            HANDLE pipe=CreateFileA(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,
                                    OPEN_EXISTING,0,nullptr);
            if(pipe!=INVALID_HANDLE_VALUE)
                return pipe;
            hgl::SleepSecond(0.01);
        }
        return INVALID_HANDLE_VALUE;
    }
}
#endif

int main()
{
    using hgl::graph::debug_bridge::Parser;
    using hgl::graph::debug_bridge::Request;
    const char valid[]="{\"jsonrpc\":\"2.0\",\"id\":\"a\\\"b\",\"method\":\"set_debug_view\","
                       "\"params\":{\"irrelevant\":\"world_normal\",\"mode\":\"\\u006fff\"}}";
    Request parsed;
    if(!Parser(valid,sizeof(valid)-1).Parse(parsed)
       || ::strcmp(parsed.method,"set_debug_view")!=0
       || ::strcmp(parsed.mode,"off")!=0
       || ::strcmp(parsed.id,"\"a\\\"b\"")!=0)
        return 10;

    const char *invalid[]={
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"off\",\"method\":\"get_debug_view\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"get_debug_view\",\"params\":{\"mode\":\"off\",\"mode\":\"uv0\"}}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"get_debug_view\"} trailing",
        "{\"jsonrpc\":\"2.0\",\"id\":01,\"method\":\"get_debug_view\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"\\uD800\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"get_debug_view\",\"params\":{\"x\":[1,]}}"
    };
    for(const char *text:invalid)
    {
        Request rejected;
        if(Parser(text,::strlen(text)).Parse(rejected))
            return 11;
    }

#ifdef _WIN32
    hgl::debug::SetEnabled(hgl::debug::Channel::DebugBridge,true);
    hgl::AnsiString name="\\\\.\\pipe\\ulre-debug-test-";
    name+=hgl::AnsiString::numberOf(static_cast<uint32_t>(GetCurrentProcessId()));
    hgl::graph::DebugBridgeServer server(name.c_str());

    if(!server.StartServer())
        return 1;
    HANDLE pipe=Connect(name);
    if(pipe==INVALID_HANDLE_VALUE)
        return 2;
    const char request[]="{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"capture_frame\"}";
    DWORD written=0;
    if(!WriteFile(pipe,request,sizeof(request)-1,&written,nullptr)
       || written!=sizeof(request)-1)
        return 3;
    char response[512]={};
    DWORD received=0;
    if(!ReadFile(pipe,response,sizeof(response)-1,&received,nullptr)
       || !::strstr(response,"missing_target_id"))
        return 4;
    CloseHandle(pipe);
    pipe=Connect(name);
    if(pipe==INVALID_HANDLE_VALUE)
        return 12;
    const char deceptive[]="{\"jsonrpc\":\"2.0\",\"id\":42,\"method\":\"unknown\","
                            "\"params\":{\"text\":\"get_render_health\"}}";
    if(!WriteFile(pipe,deceptive,sizeof(deceptive)-1,&written,nullptr)
       || written!=sizeof(deceptive)-1)
        return 13;
    ::memset(response,0,sizeof(response));
    if(!ReadFile(pipe,response,sizeof(response)-1,&received,nullptr)
       || !::strstr(response,"\"id\":42")
       || !::strstr(response,"-32601"))
        return 14;
    CloseHandle(pipe);
    pipe=Connect(name);
    if(pipe==INVALID_HANDLE_VALUE)
        return 15;
    char large[5000];
    ::memset(large,' ',sizeof(large));
    if(!WriteFile(pipe,large,sizeof(large),&written,nullptr) || written!=sizeof(large))
        return 16;
    ::memset(response,0,sizeof(response));
    if(!ReadFile(pipe,response,sizeof(response)-1,&received,nullptr)
       || !::strstr(response,"Request too large"))
        return 17;
    CloseHandle(pipe);
    server.StopServer();
    if(server.IsRunning())
        return 5;

    if(!server.StartServer())
        return 6;
    pipe=Connect(name);
    if(pipe==INVALID_HANDLE_VALUE)
        return 7;
    hgl::SleepSecond(0.05);
    server.StopServer();
    CloseHandle(pipe);
    if(server.IsRunning())
        return 8;

    if(!server.StartServer())
        return 9;
    server.StopServer();
    hgl::debug::ClearOverrides();
#endif
    return 0;
}
