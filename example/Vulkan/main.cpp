#include"VK.h"
#include"VKInstance.h"
#include"VKDevice.h"
#include"VKCommandBuffer.h"

int main(int,char **)
{
    using namespace hgl;
    using namespace hgl::graph;

    vulkan::Instance inst("Test");

    if(!inst.Init())
        return(-1);

    const ObjectList<vulkan::PhysicalDevice> &device_list=inst.GetDeviceList();

    vulkan::PhysicalDevice *pd=device_list[0];

    vulkan::Device *dev=pd->CreateGraphicsDevice();

    vulkan::CommandBuffer *cmd_buf=dev->CreateCommandBuffer();

    delete cmd_buf;
    delete dev;

    return 0;
}
