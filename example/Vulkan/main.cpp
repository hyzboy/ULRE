#include"Window.h"
#include"VKInstance.h"

using namespace hgl;
using namespace hgl::graph;

int main(int,char **)
{
    Window *win=CreateRenderWindow(OS_TEXT("VulkanTest"));

    win->Create(1280,720);

    vulkan::Instance inst(U8_TEXT("VulkanTest"),win);

    if(!inst.Init())
    {
        delete win;
        return(-1);
    }

    vulkan::RenderSurface *render=inst.CreateRenderSurface();
    vulkan::CommandBuffer *cmd_buf=render->CreateCommandBuffer();



    delete cmd_buf;
    delete render;
    delete win;

    return 0;
}
