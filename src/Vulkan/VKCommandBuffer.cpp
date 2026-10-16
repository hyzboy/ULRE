#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<cstdio>

namespace hgl::graph{
VulkanCmdBuffer::VulkanCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb,VkCommandPool pool)
{
    dev_attr=attr;
    cmd_pool=pool?pool:(attr?attr->cmd_pool:VK_NULL_HANDLE);
    cmd_buf=cb;

    cmd_begin=false;
}

VulkanCmdBuffer::~VulkanCmdBuffer()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(dev_attr->device);
    if (owner)
        owner->UntrackObject(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)(uintptr_t)cmd_buf);

    if(cmd_pool)
        vkFreeCommandBuffers(dev_attr->device,cmd_pool,1,&cmd_buf);
}

bool VulkanCmdBuffer::Begin()
{
    if(cmd_begin)
    {
        LogError("[CMD] vkBeginCommandBuffer rejected: command buffer already recording cmd=%p",
                 (void*)cmd_buf);
        return false;
    }

    CommandBufferBeginInfo cmd_buf_info;

    cmd_buf_info.pInheritanceInfo = nullptr;

    VkResult result=vkBeginCommandBuffer(cmd_buf, &cmd_buf_info);
    if(result!=VK_SUCCESS)
    {
        LogError("[CMD] vkBeginCommandBuffer failed cmd=%p result=%d",
                 (void*)cmd_buf,static_cast<int>(result));
        return(false);
    }

    cmd_begin=true;
    global_sets_bound=false;

    // 顶层调试区域：整条命令缓冲一个作用域。抓帧工具里这一层下面的所有
    // barrier / pass / draw 都归它管；由 End() 收口（正常路径在 End() 里闭合）。
    BeginRegion("cmd");

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
// ── 调试标注实现（VK_EXT_debug_utils 的 label 系列）────────────────────────────
// 只有 _DEBUG 才存在这些调用；dev_attr->debug_utils 为空（未启用校验层/抓帧层）
// 时全部安全空转——所以调用点可以无条件写，不需要环绕判断。

void VulkanCmdBuffer::SetDebugName(const AnsiString &object_name)
{
    if(dev_attr && dev_attr->debug_utils)
        dev_attr->debug_utils->SetCommandBuffer(cmd_buf,object_name);
}

void VulkanCmdBuffer::BeginRegion(const char *name,const Color4f &color)
{
    if(!dev_attr || !dev_attr->debug_utils || !name || !*name)
        return;

    dev_attr->debug_utils->CmdBegin(cmd_buf,name,color);
    ++debug_region_depth;
}

void VulkanCmdBuffer::BeginPassRegion(const char *kind,uint32_t width,uint32_t height)
{
    if(!dev_attr || !dev_attr->debug_utils)
        return;

    char name[96];

    snprintf(name,sizeof(name),"%s %ux%u",kind?kind:"pass",width,height);

    dev_attr->debug_utils->CmdBegin(cmd_buf,name,Color4f(0.30f,0.55f,0.90f,1.0f));
    ++debug_region_depth;
}

void VulkanCmdBuffer::EndRegion()
{
    if(debug_region_depth<=0)   // 未配对 ⇒ 安全空转，不产生悬空 CmdEnd
        return;

    --debug_region_depth;

    if(dev_attr && dev_attr->debug_utils)
        dev_attr->debug_utils->CmdEnd(cmd_buf);
}

void VulkanCmdBuffer::InsertMarker(const char *name,const Color4f &color)
{
    if(!dev_attr || !dev_attr->debug_utils || !name || !*name)
        return;

    dev_attr->debug_utils->CmdInsert(cmd_buf,name,color);
}
#endif//_DEBUG
}//namespace hgl::graph
