#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

namespace
{
    int first_count = 0;
    int second_count = 0;

    void FirstCallback() { ++first_count; }
    void SecondCallback() { ++second_count; }
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestGraphicsContextShutdownCallbacks"));

    {
        hgl::graph::GraphicsContext graphics(nullptr);
        graphics.AddShutdownCallback(nullptr);
        graphics.AddShutdownCallback(&FirstCallback);
        graphics.AddShutdownCallback(&FirstCallback);
        graphics.AddShutdownCallback(&SecondCallback);
        graphics.Shutdown();
        graphics.Shutdown();

        if (first_count != 1 || second_count != 1)
        {
            GLogError(u8"GraphicsContext shutdown callbacks were not called exactly once");
            return 1;
        }

        graphics.AddShutdownCallback(&FirstCallback);
    }

    if (first_count != 2 || second_count != 1)
    {
        GLogError(u8"GraphicsContext destructor did not release a re-registered callback");
        return 2;
    }

    return 0;
}
