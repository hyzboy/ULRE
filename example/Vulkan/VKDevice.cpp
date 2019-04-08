#include"VKDevice.h"
#include"VKCommandBuffer.h"

VK_NAMESPACE_BEGIN
Device::Device(VkDevice dev,int family_index)
{
    device=dev;

    cmd_pool=nullptr;

    if(!device)
        return;

    {
        VkCommandPoolCreateInfo cmd_pool_info = {};

        cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmd_pool_info.pNext = NULL;
        cmd_pool_info.queueFamilyIndex = family_index;
        cmd_pool_info.flags = 0;

        VkResult res = vkCreateCommandPool(device, &cmd_pool_info, nullptr, &cmd_pool);
    }
}

Device::~Device()
{
    vkDestroyCommandPool(device,cmd_pool,nullptr);
    vkDestroyDevice(device,nullptr);
}

CommandBuffer *Device::CreateCommandBuffer()
{
    if(!cmd_pool)
        return(nullptr);

    VkCommandBufferAllocateInfo cmd = {};
    cmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext = nullptr;
    cmd.commandPool = cmd_pool;
    cmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount = 1;

    VkCommandBuffer cmd_buf;

    VkResult res = vkAllocateCommandBuffers(device, &cmd, &cmd_buf);

    if(res!=VK_SUCCESS)
        return(nullptr);

    return(new CommandBuffer(device,cmd_pool,cmd_buf));
}
VK_NAMESPACE_END
