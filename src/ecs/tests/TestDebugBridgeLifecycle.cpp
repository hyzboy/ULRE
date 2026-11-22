#include<hgl/vk/DebugBridgeServer.h>
#include<hgl/common/DebugChannel.h>
#include<hgl/time/Time.h>
#include<hgl/log/Log.h>
#include<cstring>

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
       || !::strstr(response,"capture_request_not_supported"))
        return 4;
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
