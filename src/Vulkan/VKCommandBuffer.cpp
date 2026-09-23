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

void VulkanCmdBuffer::PipelineBarrier2(const VkDependencyInfo *dep_info)
{
    if(dev_attr && dev_attr->cmd_pipeline_barrier2 && dep_info)
        dev_attr->cmd_pipeline_barrier2(cmd_buf, dep_info);
}

void VulkanCmdBuffer::PipelineBarrier2(const VkDependencyInfo &dep_info)
{
    PipelineBarrier2(&dep_info);
}

void VulkanCmdBuffer::MemoryBarrier2(VkPipelineStageFlags2 src_stage,
                                    VkPipelineStageFlags2 dst_stage,
                                    VkAccessFlags2 src_access,
                                    VkAccessFlags2 dst_access)
{
    if(!dev_attr || !dev_attr->cmd_pipeline_barrier2)
        return;

    VkMemoryBarrier2 barrier{};
    barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    barrier.pNext         = nullptr;
    barrier.srcStageMask  = src_stage;
    barrier.srcAccessMask = src_access;
    barrier.dstStageMask  = dst_stage;
    barrier.dstAccessMask = dst_access;

    VkDependencyInfo dep_info{};
    dep_info.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.pNext                    = nullptr;
    dep_info.memoryBarrierCount       = 1;
    dep_info.pMemoryBarriers          = &barrier;

    dev_attr->cmd_pipeline_barrier2(cmd_buf, &dep_info);
}

void VulkanCmdBuffer::BufferMemoryBarrier2(VkBuffer buffer,
                                          VkPipelineStageFlags2 srcStageMask,
                                          VkPipelineStageFlags2 dstStageMask,
                                          VkAccessFlags2 srcAccessMask,
                                          VkAccessFlags2 dstAccessMask,
                                          VkDeviceSize offset,
                                          VkDeviceSize size)
{
    if(!dev_attr || !dev_attr->cmd_pipeline_barrier2)
        return;

    VkBufferMemoryBarrier2 barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    barrier.pNext               = nullptr;
    barrier.srcStageMask        = srcStageMask;
    barrier.srcAccessMask       = srcAccessMask;
    barrier.dstStageMask        = dstStageMask;
    barrier.dstAccessMask       = dstAccessMask;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer              = buffer;
    barrier.offset              = offset;
    barrier.size                = size;

    VkDependencyInfo dep_info{};
    dep_info.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.pNext                    = nullptr;
    dep_info.bufferMemoryBarrierCount = 1;
    dep_info.pBufferMemoryBarriers    = &barrier;

    dev_attr->cmd_pipeline_barrier2(cmd_buf, &dep_info);
}

void VulkanCmdBuffer::ImageMemoryBarrier2(VkImage image,
                                         VkPipelineStageFlags2 srcStageMask,
                                         VkPipelineStageFlags2 dstStageMask,
                                         VkAccessFlags2 srcAccessMask,
                                         VkAccessFlags2 dstAccessMask,
                                         VkImageLayout oldImageLayout,
                                         VkImageLayout newImageLayout,
                                         const VkImageSubresourceRange &subresourceRange)
{
    if(!dev_attr || !dev_attr->cmd_pipeline_barrier2)
        return;

    VkImageMemoryBarrier2 barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.pNext               = nullptr;
    barrier.srcStageMask        = srcStageMask;
    barrier.srcAccessMask       = srcAccessMask;
    barrier.dstStageMask        = dstStageMask;
    barrier.dstAccessMask       = dstAccessMask;
    barrier.oldLayout           = oldImageLayout;
    barrier.newLayout           = newImageLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = subresourceRange;

    VkDependencyInfo dep_info{};
    dep_info.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.pNext                   = nullptr;
    dep_info.imageMemoryBarrierCount = 1;
    dep_info.pImageMemoryBarriers    = &barrier;

    dev_attr->cmd_pipeline_barrier2(cmd_buf, &dep_info);
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
