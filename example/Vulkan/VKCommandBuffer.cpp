#include"VKCommandBuffer.h"

VK_NAMESPACE_BEGIN
CommandBuffer::~CommandBuffer()
{
    VkCommandBuffer cmd_bufs[1] = {buf};
    vkFreeCommandBuffers(device, pool, 1, cmd_bufs);
}
VK_NAMESPACE_END
