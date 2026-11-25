#include<hgl/vk/DebugBridgeServer.h>
#include<hgl/ecs/support/debug/DebugViewController.h>
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
    using hgl::graph::debug_bridge::Request;
    const char valid[]="{\"jsonrpc\":\"2.0\",\"id\":\"a\\\"b\",\"method\":\"set_debug_view\","
                       "\"params\":{\"irrelevant\":\"world_normal\",\"mode\":\"\\u006fff\"}}";
    Request parsed;
    if(!parsed.Parse(valid,sizeof(valid)-1)
       || parsed.GetMethod()!="set_debug_view"
       || parsed.GetParamString("mode")!="off"
       || parsed.GetId()!=nlohmann::json("a\"b"))
        return 10;

    const char buffer_request[]="{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"capture_buffer\","
                                "\"params\":{\"resource_id\":12,\"offset\":0,\"size\":16}}";
    Request buffer;
    uint64_t resource_id=0,offset=0,size=0;
    if(!buffer.Parse(buffer_request,sizeof(buffer_request)-1)
       || !buffer.GetParamUInt64("resource_id",resource_id) || resource_id!=12
       || !buffer.GetParamUInt64("offset",offset) || offset!=0
       || !buffer.GetParamUInt64("size",size) || size!=16)
        return 10;

    // 回归：顶层 id 与 params 内实体参数共存（旧解析器共享 has_id，此请求必然失败）
    const char trace_request[]="{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"trace_entity_pipeline\","
                               "\"params\":{\"name\":\"chess\"}}";
    Request trace;
    if(!trace.Parse(trace_request,sizeof(trace_request)-1)
       || trace.GetId()!=nlohmann::json(7)
       || trace.GetEntity()!="chess")
        return 18;

    // 回归：\uXXXX 正确解码为 UTF-8（旧实现将非 ASCII 截断为 0xFF）；id:null 合法
    const char unicode_request[]="{\"jsonrpc\":\"2.0\",\"id\":null,\"method\":\"trace_entity_render_state\","
                                 "\"params\":{\"entity\":\"\\u68cb\\u76d8\"}}";
    Request unicode;
    if(!unicode.Parse(unicode_request,sizeof(unicode_request)-1)
       || !unicode.GetId().is_null())
        return 18;
    const hgl::AnsiString entity=unicode.GetEntity();
    if(entity.Length()!=6 || ::memcmp(entity.c_str(),"\xe6\xa3\x8b\xe7\x9b\x98",6)!=0)
        return 18;

    const char *invalid[]={
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"off\",\"method\":\"get_debug_view\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"get_debug_view\",\"params\":{\"mode\":\"off\",\"mode\":\"uv0\"}}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"get_debug_view\"} trailing",
        "{\"jsonrpc\":\"2.0\",\"id\":01,\"method\":\"get_debug_view\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"\\uD800\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"get_debug_view\",\"params\":{\"x\":[1,]}}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"capture_buffer\",\"params\":{\"offset\":-1}}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"capture_buffer\",\"params\":{\"size\":1,\"size\":4}}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"capture_buffer\",\"params\":{\"size\":18446744073709551616}}",
        "{\"jsonrpc\":\"2.0\",\"id\":true,\"method\":\"get_debug_view\"}",
        "{\"jsonrpc\":\"2.0\",\"method\":\"get_debug_view\"}",
        "{\"jsonrpc\":\"1.0\",\"id\":1,\"method\":\"get_debug_view\"}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"get_debug_view\",\"params\":[]}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"get_debug_view\",\"params\":{\"x\":1,\"x\":2}}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"trace_entity_pipeline\",\"params\":{\"name\":\"a\",\"entity\":\"b\"}}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"capture_buffer\",\"params\":{\"offset\":1.5}}",
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"capture_buffer\",\"params\":{\"offset\":\"1\"}}"
    };
    for(const char *text:invalid)
    {
        Request rejected;
        if(rejected.Parse(text,::strlen(text)))
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
        return 20;
    const char set_view[]="{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"set_debug_view\","
                          "\"params\":{\"mode\":\"world_normal\"}}";
    if(!WriteFile(pipe,set_view,sizeof(set_view)-1,&written,nullptr)
       || written!=sizeof(set_view)-1)
        return 21;
    ::memset(response,0,sizeof(response));
    if(!ReadFile(pipe,response,sizeof(response)-1,&received,nullptr)
       || !::strstr(response,"\"requested_mode\":\"world_normal\"")
       || hgl::ecs::DebugViewController::GetRequestedMode()!=hgl::graph::mtl::DebugViewMode::WorldNormal)
        return 22;
    CloseHandle(pipe);
    pipe=Connect(name);
    if(pipe==INVALID_HANDLE_VALUE)
        return 23;
    const char invalid_view[]="{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"set_offscreen_debug_view\","
                              "\"params\":{\"mode\":\"unknown\"}}";
    if(!WriteFile(pipe,invalid_view,sizeof(invalid_view)-1,&written,nullptr)
       || written!=sizeof(invalid_view)-1)
        return 24;
    ::memset(response,0,sizeof(response));
    if(!ReadFile(pipe,response,sizeof(response)-1,&received,nullptr)
       || !::strstr(response,"unknown_debug_view_mode")
       || hgl::ecs::DebugViewController::GetRequestedMode()!=hgl::graph::mtl::DebugViewMode::WorldNormal)
        return 25;
    CloseHandle(pipe);
    if(!hgl::ecs::DebugViewController::RequestMode(hgl::graph::mtl::DebugViewMode::Off))
        return 26;
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
        return 19;
    const char trace_live[]="{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"trace_entity_pipeline\","
                            "\"params\":{\"name\":\"chess\"}}";
    if(!WriteFile(pipe,trace_live,sizeof(trace_live)-1,&written,nullptr)
       || written!=sizeof(trace_live)-1)
        return 19;
    ::memset(response,0,sizeof(response));
    if(!ReadFile(pipe,response,sizeof(response)-1,&received,nullptr)
       || !::strstr(response,"\"id\":9")
       || !::strstr(response,"no pipeline inspection data available")
       || ::strstr(response,"-32600"))
        return 19;
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
