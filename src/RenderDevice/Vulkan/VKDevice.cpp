#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/type/Pair.h>
#include<hgl/graph/vulkan/VKSemaphore.h>
#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKImageView.h>
#include<hgl/graph/vulkan/VKCommandBuffer.h>
//#include<hgl/graph/vulkan/VKDescriptorSet.h>
#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKShaderModuleManage.h>
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
    swapchain=CreateSwapchain(extent);

    texture_cmd_buf=CreateCommandBuffer(extent,0);
    textureSQ=new SubmitQueue(this,attr->graphics_queue,1);

    swapchainRT=new SwapchainRenderTarget(this,swapchain);

    return(true);
}

CommandBuffer *Device::CreateCommandBuffer(const VkExtent2D &extent,const uint32_t atta_count)
{
    if(!attr->cmd_pool)
        return(nullptr);

    VkCommandBufferAllocateInfo cmd;
    cmd.sType               =VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext               =nullptr;
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
    VkFenceCreateInfo fenceInfo;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = create_signaled?VK_FENCE_CREATE_SIGNALED_BIT:0;

    VkFence fence;

    if(vkCreateFence(attr->device, &fenceInfo, nullptr, &fence)!=VK_SUCCESS)
        return(nullptr);

    return(new Fence(attr->device,fence));
}

vulkan::Semaphore *Device::CreateSem()
{
    VkSemaphoreCreateInfo SemaphoreCreateInfo;
    SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    SemaphoreCreateInfo.pNext = nullptr;
    SemaphoreCreateInfo.flags = 0;

    VkSemaphore sem;
    if(vkCreateSemaphore(attr->device, &SemaphoreCreateInfo, nullptr, &sem)!=VK_SUCCESS)
        return(nullptr);

    return(new vulkan::Semaphore(attr->device,sem));
}

ShaderModuleManage *Device::CreateShaderModuleManage()
{
    return(new ShaderModuleManage(this));
}

RenderTarget *Device::CreateRenderTarget(Framebuffer *fb)
{
    return(new RenderTarget(this,fb));
}
VK_NAMESPACE_END
