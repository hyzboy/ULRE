#include<hgl/graph/vulkan/VKRenderTarget.h>
#include"VulkanAppFramework.h"

using namespace hgl;
using namespace hgl::graph;

constexpr uint OFFSCREEN_SIZE   =512;
constexpr uint SCREEN_WIDTH     =1024;
constexpr uint SCREEN_HEIGHT    =(SCREEN_WIDTH/16)*9;

class OffscreenRender:public VulkanApplicationFramework
{
    vulkan::RenderTarget *os_rt;

public:

    ~OffscreenRender()
    {
    }

    bool InitOffscreenRT()
    {
        os_rt=vulkan::CreateColorFramebuffer(
    }

    bool Init()
    {
        if(!VulkanApplicationFramework::Init(SCREEN_WIDTH,SCREEN_HEIGHT))
            return(false);

        if(!InitOffscreenRT())
            return(false);

        return(true);
    }
};//class OffscreenRender:public VulkanApplicationFramework

int main(int,char **)
{
    OffscreenRender app;

    if(!app.Init())
        return(-1);

    while(app.Run());

    return 0;
}
