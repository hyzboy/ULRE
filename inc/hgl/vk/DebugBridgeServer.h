#pragma once

#include<hgl/thread/Thread.h>
#include<hgl/type/String.h>

#ifdef _WIN32
#include<Windows.h>
#endif

namespace hgl::graph
{
class DebugBridgeServer : public hgl::Thread
{
#ifdef _WIN32
    HANDLE pipe_handle = INVALID_HANDLE_VALUE;
    HANDLE stop_event = nullptr;
#endif
    AnsiString pipe_name;

protected:
    bool ProcStartThread() override;
    void ProcEndThread() override;
    void OnStopRequested() override;

public:
    explicit DebugBridgeServer(const char *name="\\\\.\\pipe\\ulre_render_debug");
    ~DebugBridgeServer() override;

    bool Execute() override;
    bool StartServer();
    void StopServer();
    static bool StartActive();
    static void StopActive();
};
}//namespace hgl::graph
