#pragma once

#include<hgl/thread/Thread.h>

#ifdef _WIN32
#include<Windows.h>
#endif

namespace hgl::graph
{
class DebugBridgeServer : public hgl::Thread
{
#ifdef _WIN32
    HANDLE pipe_handle = INVALID_HANDLE_VALUE;
#endif
    bool started = false;

protected:
    bool ProcStartThread() override;
    void ProcEndThread() override;
    void OnStopRequested() override;

public:
    ~DebugBridgeServer() override;

    bool Execute() override;
    bool StartServer();
    void StopServer();
    static bool StartActive();
    static void StopActive();
};
}//namespace hgl::graph
