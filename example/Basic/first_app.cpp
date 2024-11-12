#include<hgl/graph/RenderFramework.h>

using namespace hgl;
using namespace hgl::graph;

int os_main(int,os_char **)
{
    RenderFramework rf;

    if(!rf.Init(1280,720,OS_TEXT("FirstApp")))
        return 1;

    rf.Run();

    return 0;
}