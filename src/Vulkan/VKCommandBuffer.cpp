#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKDeviceAttribute.h>

namespace hgl::graph{
VulkanCmdBuffer::VulkanCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb)
{
    dev_attr=attr;
    cmd_buf=cb;

    cmd_begin=false;
}

VulkanCmdBuffer::~VulkanCmdBuffer()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(dev_attr->device);
    if (owner)
        owner->UntrackObject(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)(uintptr_t)cmd_buf);

    vkFreeCommandBuffers(dev_attr->device,dev_attr->cmd_pool,1,&cmd_buf);
}

bool VulkanCmdBuffer::Begin()
{
    CommandBufferBeginInfo cmd_buf_info;

    cmd_buf_info.pInheritanceInfo = nullptr;

    if(vkBeginCommandBuffer(cmd_buf, &cmd_buf_info)!=VK_SUCCESS)
        return(false);

    cmd_begin=true;
    scene_sets_bound=false;
    return(true);
}

void VulkanCmdBuffer::BufferMemoryBarrier(VkBuffer buffer,
                                         VkPipelineStageFlags srcStageMask,
                                         VkPipelineStageFlags dstStageMask,
                                         VkAccessFlags srcAccessMask,
                                         VkAccessFlags dstAccessMask,
                                         VkDeviceSize offset,
                                         VkDeviceSize size)
{
    VkBufferMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.pNext               = nullptr;
    barrier.srcAccessMask       = srcAccessMask;
    barrier.dstAccessMask       = dstAccessMask;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer              = buffer;
    barrier.offset              = offset;
    barrier.size                = size;

    vkCmdPipelineBarrier(cmd_buf,
                         srcStageMask,
                         dstStageMask,
                         0,
                         0, nullptr,
                         1, &barrier,
                         0, nullptr);
}

#ifdef _DEBUG
void VulkanCmdBuffer::SetDebugName(const AnsiString &object_name)
{
    if(dev_attr->debug_utils)
        dev_attr->debug_utils->SetCommandBuffer(cmd_buf,object_name);
}

void VulkanCmdBuffer::BeginRegion(const AnsiString &region_name,const Color4f &color)
{
    if(dev_attr->debug_utils)
        dev_attr->debug_utils->CmdBegin(cmd_buf,region_name,color);
}

void VulkanCmdBuffer::EndRegion()
{
    if(dev_attr->debug_utils)
        dev_attr->debug_utils->CmdEnd(cmd_buf);
}
#endif
}//namespace hgl::graph
