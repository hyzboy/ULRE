#include<hgl/graph/VKDevice.h>
#include<hgl/type/Pair.h>
#include<hgl/graph/VKSemaphore.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKCommandBuffer.h>
//#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKDescriptorSets.h>

VK_NAMESPACE_BEGIN
GPUDevice::GPUDevice(GPUDeviceAttribute *da)
{
    attr=da;

    texture_queue=nullptr;
    texture_cmd_buf=nullptr;

    swapchain=nullptr;
    swapchainRT=nullptr;
    Resize(attr->surface_caps.currentExtent);
}

GPUDevice::~GPUDevice()
{
    SAFE_CLEAR(swapchainRT);
    SAFE_CLEAR(swapchain);

    SAFE_CLEAR(texture_queue);
    SAFE_CLEAR(texture_cmd_buf);

    delete attr;
}

bool GPUDevice::Resize(const VkExtent2D &extent)
{
    SAFE_CLEAR(swapchainRT);
    SAFE_CLEAR(swapchain);

    SAFE_CLEAR(texture_queue);
    SAFE_CLEAR(texture_cmd_buf);

    attr->Refresh();

    if(!CreateSwapchain(extent))
        return(false);

    texture_cmd_buf=CreateTextureCommandBuffer();
    texture_queue=new GPUQueue(this,attr->graphics_queue,1);

    swapchainRT=new SwapchainRenderTarget(this,swapchain);

    return(true);
}

VkCommandBuffer GPUDevice::CreateCommandBuffer()
{
    if(!attr->cmd_pool)
        return(VK_NULL_HANDLE);

    CommandBufferAllocateInfo cmd;

    cmd.commandPool         =attr->cmd_pool;
    cmd.level               =VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount  =1;

    VkCommandBuffer cmd_buf;

    VkResult res=vkAllocateCommandBuffers(attr->device,&cmd,&cmd_buf);

    if(res!=VK_SUCCESS)
        return(VK_NULL_HANDLE);

    return cmd_buf;
}

RenderCmdBuffer *GPUDevice::CreateRenderCommandBuffer()
{
    VkCommandBuffer cb=CreateCommandBuffer();

    if(cb==VK_NULL_HANDLE)return(nullptr);

    return(new RenderCmdBuffer(attr,cb));
}

TextureCmdBuffer *GPUDevice::CreateTextureCommandBuffer()
{
    VkCommandBuffer cb=CreateCommandBuffer();

    if(cb==VK_NULL_HANDLE)return(nullptr);

    return(new TextureCmdBuffer(attr,cb));
}

/**
 * 创建栅栏
 * @param create_signaled 是否创建初始信号
 */
GPUFence *GPUDevice::CreateFence(bool create_signaled)
{
    FenceCreateInfo fenceInfo(create_signaled?VK_FENCE_CREATE_SIGNALED_BIT:0);

    VkFence fence;

    if(vkCreateFence(attr->device, &fenceInfo, nullptr, &fence)!=VK_SUCCESS)
        return(nullptr);

    return(new GPUFence(attr->device,fence));
}

GPUSemaphore *GPUDevice::CreateGPUSemaphore()
{
    SemaphoreCreateInfo SemaphoreCreateInfo;

    VkSemaphore sem;

    if(vkCreateSemaphore(attr->device, &SemaphoreCreateInfo, nullptr, &sem)!=VK_SUCCESS)
        return(nullptr);

    return(new GPUSemaphore(attr->device,sem));
}
VK_NAMESPACE_END
