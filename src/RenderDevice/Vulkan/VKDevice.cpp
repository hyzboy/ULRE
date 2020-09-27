#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/type/Pair.h>
#include<hgl/graph/vulkan/VKSemaphore.h>
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKImageView.h>
#include<hgl/graph/vulkan/VKPipeline.h>
#include<hgl/graph/vulkan/VKCommandBuffer.h>
//#include<hgl/graph/vulkan/VKDescriptorSet.h>
#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKDescriptorSets.h>

VK_NAMESPACE_BEGIN
Device::Device(DeviceAttribute *da)
{
    attr=da;

    textureSQ=nullptr;
    texture_cmd_buf=nullptr;

    swapchain=nullptr;
    swapchainRT=nullptr;
    Resize(attr->surface_caps.currentExtent);
}

Device::~Device()
{
    SAFE_CLEAR(swapchainRT);
    SAFE_CLEAR(swapchain);

    SAFE_CLEAR(textureSQ);
    SAFE_CLEAR(texture_cmd_buf);

    delete attr;
}

bool Device::Resize(const VkExtent2D &extent)
{
    SAFE_CLEAR(swapchainRT);
    SAFE_CLEAR(swapchain);

    SAFE_CLEAR(textureSQ);
    SAFE_CLEAR(texture_cmd_buf);

    attr->Refresh();

    if(!CreateSwapchain(extent))
        return(false);

    texture_cmd_buf=CreateCommandBuffer(extent,0);
    textureSQ=new SubmitQueue(this,attr->graphics_queue,1);

    swapchainRT=new SwapchainRenderTarget(this,swapchain);

    return(true);
}

CommandBuffer *Device::CreateCommandBuffer(const VkExtent2D &extent,const uint32_t atta_count)
{
    if(!attr->cmd_pool)
        return(nullptr);

    CommandBufferAllocateInfo cmd;

    cmd.commandPool         =attr->cmd_pool;
    cmd.level               =VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount  =1;

    VkCommandBuffer cmd_buf;

    VkResult res=vkAllocateCommandBuffers(attr->device,&cmd,&cmd_buf);

    if(res!=VK_SUCCESS)
        return(nullptr);

    return(new CommandBuffer(attr->device,extent,atta_count,attr->cmd_pool,cmd_buf));
}

/**
 * 创建栅栏
 * @param create_signaled 是否创建初始信号
 */
Fence *Device::CreateFence(bool create_signaled)
{
    FenceCreateInfo fenceInfo(create_signaled?VK_FENCE_CREATE_SIGNALED_BIT:0);

    VkFence fence;

    if(vkCreateFence(attr->device, &fenceInfo, nullptr, &fence)!=VK_SUCCESS)
        return(nullptr);

    return(new Fence(attr->device,fence));
}

vulkan::Semaphore *Device::CreateSem()
{
    SemaphoreCreateInfo SemaphoreCreateInfo;

    VkSemaphore sem;

    if(vkCreateSemaphore(attr->device, &SemaphoreCreateInfo, nullptr, &sem)!=VK_SUCCESS)
        return(nullptr);

    return(new vulkan::Semaphore(attr->device,sem));
}

RenderTarget *Device::CreateRenderTarget(Framebuffer *fb)
{
    return(new RenderTarget(this,fb));
}

Pipeline *CreatePipeline(VkDevice device,VkPipelineCache pipeline_cache,PipelineData *data,const Material *material,const RenderTarget *rt);

Pipeline *Device::CreatePipeline(PipelineData *pd,const Material *mtl,const RenderTarget *rt)
{
    return VK_NAMESPACE::CreatePipeline(attr->device,attr->pipeline_cache,pd,mtl,rt);
}
VK_NAMESPACE_END
