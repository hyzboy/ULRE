#include"Window.h"
#include"VKInstance.h"
#include"RenderSurface.h"

using namespace hgl;
using namespace hgl::graph;

int main(int,char **)
{
    Window *win=CreateRenderWindow(OS_TEXT("VulkanTest"));

    win->Create(1280,720);

    vulkan::Instance *inst=vulkan::CreateInstance(U8_TEXT("VulkanTest"));

    if(!inst)
    {
        delete win;
        return(-1);
    }

    vulkan::RenderSurface *render=inst->CreateSurface(win);
    vulkan::CommandBuffer *cmd_buf=render->CreateCommandBuffer();

    vulkan::Buffer *ubo=render->CreateUBO(1024);

    uint8_t *p=ubo->Map();

    if(p)
    {
        memset(p,0,1024);
        ubo->Unmap();
    }

    delete ubo;

    delete cmd_buf;
    delete render;
    delete win;

    return 0;
}
