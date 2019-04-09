#include"VK.h"
#include"VKInstance.h"
#include"VKDevice.h"
#include"VKCommandBuffer.h"
#include"Window.h"

using namespace hgl;
using namespace hgl::graph;

int main(int,char **)
{
    Window *win=CreateRenderWindow(OS_TEXT("VulkanTest"));

    vulkan::Instance inst(U8_TEXT("VulkanTest"),win);

    if(!inst.Init())
    {
        delete win;
        return(-1);
    }

    const ObjectList<vulkan::PhysicalDevice> &device_list=inst.GetDeviceList();

    vulkan::PhysicalDevice *pd=device_list[0];

    vulkan::Device *dev=pd->CreateGraphicsDevice();

    vulkan::CommandBuffer *cmd_buf=dev->CreateCommandBuffer();

    delete cmd_buf;
    delete dev;
    delete win;

    return 0;
}
