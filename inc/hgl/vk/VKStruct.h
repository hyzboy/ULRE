#pragma once
#include <vulkan/vulkan.h>

namespace hgl::graph{
template<typename T,VkStructureType ST> struct vkstruct:public T
{
public:

    vkstruct()
    {
        this->sType = ST;
        this->pNext = nullptr;
    }

    virtual ~vkstruct()=default;
};//

template<typename T,VkStructureType ST> struct vkstruct_flag:public T
{
public:

    vkstruct_flag()
    {
        this->sType = ST;
        this->pNext = nullptr;
        this->flags = 0;
    }

    vkstruct_flag(VkFlags flags)
    {
        this->sType = ST;
        this->pNext = nullptr;
        this->flags = flags;
    }

    virtual ~vkstruct_flag()=default;
};//

#define VKS_DEFINE(name,value)  using name=vkstruct<Vk##name,value>;
#define VKSF_DEFINE(name,value)  using name=vkstruct_flag<Vk##name,value>;

#define VKS_DEFINE_KHR(name,value)  using name=vkstruct<Vk##name##KHR,value>;
#define VKSF_DEFINE_KHR(name,value)  using name=vkstruct_flag<Vk##name##KHR,value>;

VKS_DEFINE(     ApplicationInfo,                VK_STRUCTURE_TYPE_APPLICATION_INFO)

struct InstanceCreateInfo:public vkstruct_flag<VkInstanceCreateInfo,VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO>
{
public:

    InstanceCreateInfo(VkApplicationInfo *ai)
    {
        this->pApplicationInfo=ai;
    }
};

struct MemoryAllocateInfo:public vkstruct<VkMemoryAllocateInfo,VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO>
{
public:

    MemoryAllocateInfo(const uint32_t index,const VkDeviceSize size)
    {
        memoryTypeIndex  =index;
        allocationSize   =size;
    }
};

VKSF_DEFINE(    PipelineCacheCreateInfo,        VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO)
VKSF_DEFINE(    FramebufferCreateInfo,          VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO)

VKSF_DEFINE(    DescriptorSetLayoutCreateInfo,  VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO)
VKSF_DEFINE(    PipelineLayoutCreateInfo,       VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO)
VKS_DEFINE(     DescriptorSetAllocateInfo,      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO)

VKS_DEFINE(     CommandBufferAllocateInfo,      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO)
VKSF_DEFINE(    CommandBufferBeginInfo,         VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO)
VKS_DEFINE(     RenderPassBeginInfo,            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO)

VKS_DEFINE_KHR( PresentInfo,                    VK_STRUCTURE_TYPE_PRESENT_INFO_KHR)

VKSF_DEFINE(    FenceCreateInfo,                VK_STRUCTURE_TYPE_FENCE_CREATE_INFO)
VKSF_DEFINE(    SemaphoreCreateInfo,            VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO)

VKSF_DEFINE(    SamplerCreateInfo,              VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO)

VKSF_DEFINE(    BufferCreateInfo,               VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO)
VKSF_DEFINE(    ImageViewCreateInfo,            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO)

struct ImageSubresourceRange:public VkImageSubresourceRange
{
public:

    ImageSubresourceRange(VkImageAspectFlags aspect_mask,const uint32_t level_count=1,const uint32_t layer_count=1)
    {
        this->aspectMask    =aspect_mask;
        this->baseMipLevel  =0;
        this->levelCount    =level_count;
        this->baseArrayLayer=0;
        this->layerCount    =layer_count;
    }
};//struct ImageSubresourceRange:public VkImageSubresourceRange

struct MemoryBarrier2 : public vkstruct<VkMemoryBarrier2, VK_STRUCTURE_TYPE_MEMORY_BARRIER_2>
{
    MemoryBarrier2() = default;
    MemoryBarrier2(VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                   VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access)
    {
        this->srcStageMask  = src_stage;
        this->srcAccessMask = src_access;
        this->dstStageMask  = dst_stage;
        this->dstAccessMask = dst_access;
    }
};

struct BufferMemoryBarrier2 : public vkstruct<VkBufferMemoryBarrier2, VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2>
{
    BufferMemoryBarrier2()
    {
        this->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        this->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }

    BufferMemoryBarrier2(VkBuffer buf,
                         VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                         VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access,
                         VkDeviceSize buf_offset = 0, VkDeviceSize buf_size = VK_WHOLE_SIZE)
    {
        this->srcStageMask        = src_stage;
        this->srcAccessMask       = src_access;
        this->dstStageMask        = dst_stage;
        this->dstAccessMask       = dst_access;
        this->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        this->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        this->buffer              = buf;
        this->offset              = buf_offset;
        this->size                = buf_size;
    }
};

struct ImageMemoryBarrier2 : public vkstruct<VkImageMemoryBarrier2, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2>
{
    ImageMemoryBarrier2()
    {
        this->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        this->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }

    ImageMemoryBarrier2(VkImage img)
    {
        this->image               = img;
        this->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        this->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }

    ImageMemoryBarrier2(VkImage img,
                        VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                        VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access,
                        VkImageLayout old_layout, VkImageLayout new_layout,
                        const VkImageSubresourceRange &range)
    {
        this->srcStageMask        = src_stage;
        this->srcAccessMask       = src_access;
        this->dstStageMask        = dst_stage;
        this->dstAccessMask       = dst_access;
        this->oldLayout           = old_layout;
        this->newLayout           = new_layout;
        this->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        this->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        this->image               = img;
        this->subresourceRange    = range;
    }
};

struct DependencyInfo : public vkstruct<VkDependencyInfo, VK_STRUCTURE_TYPE_DEPENDENCY_INFO>
{
    DependencyInfo() = default;

    DependencyInfo(uint32_t img_count, const VkImageMemoryBarrier2 *img_barriers)
    {
        this->imageMemoryBarrierCount = img_count;
        this->pImageMemoryBarriers    = img_barriers;
    }

    DependencyInfo(uint32_t buf_count, const VkBufferMemoryBarrier2 *buf_barriers)
    {
        this->bufferMemoryBarrierCount = buf_count;
        this->pBufferMemoryBarriers    = buf_barriers;
    }

    DependencyInfo(uint32_t mem_count, const VkMemoryBarrier2 *mem_barriers)
    {
        this->memoryBarrierCount = mem_count;
        this->pMemoryBarriers    = mem_barriers;
    }
};

struct CommandBufferSubmitInfo : public vkstruct<VkCommandBufferSubmitInfo, VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO>
{
    CommandBufferSubmitInfo() = default;
    CommandBufferSubmitInfo(VkCommandBuffer cb, uint32_t mask = 0)
    {
        this->commandBuffer = cb;
        this->deviceMask    = mask;
    }
};

struct SemaphoreSubmitInfo : public vkstruct<VkSemaphoreSubmitInfo, VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO>
{
    SemaphoreSubmitInfo() = default;
    SemaphoreSubmitInfo(VkSemaphore sem, VkPipelineStageFlags2 stage, uint64_t val = 0, uint32_t dev_idx = 0)
    {
        this->semaphore   = sem;
        this->value       = val;
        this->stageMask   = stage;
        this->deviceIndex = dev_idx;
    }
};

struct SubmitInfo2 : public vkstruct<VkSubmitInfo2, VK_STRUCTURE_TYPE_SUBMIT_INFO_2>
{
    SubmitInfo2() = default;
};

struct SubpassDescription:public VkSubpassDescription
{
public:

    SubpassDescription(const VkAttachmentReference *color_ref_list,const uint32_t color_count,const VkAttachmentReference *depth_ref=nullptr)
    {
        flags                    =0;
        pipelineBindPoint        =VK_PIPELINE_BIND_POINT_GRAPHICS;

        inputAttachmentCount     =0;
        pInputAttachments        =nullptr;

        colorAttachmentCount     =color_count;
        pColorAttachments        =color_ref_list;

        pResolveAttachments      =nullptr;

        pDepthStencilAttachment  =depth_ref;

        preserveAttachmentCount  =0;
        pPreserveAttachments     =nullptr;
    }

    SubpassDescription(const VkAttachmentReference *color_ref,const VkAttachmentReference *depth_ref=nullptr):SubpassDescription(color_ref,1,depth_ref){}
    ~SubpassDescription()=default;
};//

struct PipelineShaderStageCreateInfo:public vkstruct_flag<VkPipelineShaderStageCreateInfo,VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO>
{
public:

    PipelineShaderStageCreateInfo(VkShaderStageFlagBits bits)
    {
        pSpecializationInfo =nullptr;
        stage               =bits;
        module              =nullptr;
        pName               ="main";
    }
};
}//namespace hgl::graph
