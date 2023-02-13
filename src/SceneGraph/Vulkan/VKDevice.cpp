#include<hgl/graph/VKDevice.h>
#include<hgl/type/Pair.h>
#include<hgl/graph/VKSemaphore.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKRenderPass.h>
#include<hgl/graph/VKFramebuffer.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKDeviceRenderPassManage.h>

VK_NAMESPACE_BEGIN
void LogSurfaceFormat(const List<VkSurfaceFormatKHR> &surface_format_list)
{
    const uint32_t format_count=surface_format_list.GetCount();
    const VkSurfaceFormatKHR *sf=surface_format_list.GetData();

    LOG_INFO(OS_TEXT("Current physics device support ")+OSString::valueOf(format_count)+OS_TEXT(" surface format"));

    const VulkanFormat *vf;
    const VulkanColorSpace *cs;

    for(uint32_t i=0;i<format_count;i++)
    {
        vf=GetVulkanFormat(sf->format);
        cs=GetVulkanColorSpace(sf->colorSpace);

        LOG_INFO("  "+AnsiString::valueOf(i)+": "+AnsiString(vf->name)+", "+AnsiString(cs->name));

        ++sf;
    }
}

GPUDevice::GPUDevice(GPUDeviceAttribute *da)
{
    attr=da;

    texture_queue=nullptr;
    texture_cmd_buf=nullptr;

    InitRenderPassManage();

    swapchainRT=nullptr;
    Resize(attr->surface_caps.currentExtent);

    texture_cmd_buf=CreateTextureCommandBuffer();
    texture_queue=CreateQueue();

    LogSurfaceFormat(attr->surface_formats_list);
}

GPUDevice::~GPUDevice()
{
    ClearRenderPassManage();

    SAFE_CLEAR(swapchainRT);

    SAFE_CLEAR(texture_queue);
    SAFE_CLEAR(texture_cmd_buf);

    delete attr;
}

bool GPUDevice::Resize(const VkExtent2D &extent)
{
    SAFE_CLEAR(swapchainRT);

    attr->RefreshSurfaceCaps();

    swapchainRT=CreateSwapchainRenderTarget();

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
Fence *GPUDevice::CreateFence(bool create_signaled)
{
    FenceCreateInfo fenceInfo(create_signaled?VK_FENCE_CREATE_SIGNALED_BIT:0);

    VkFence fence;

    if(vkCreateFence(attr->device, &fenceInfo, nullptr, &fence)!=VK_SUCCESS)
        return(nullptr);

    return(new Fence(attr->device,fence));
}

Semaphore *GPUDevice::CreateGPUSemaphore()
{
    SemaphoreCreateInfo SemaphoreCreateInfo;

    VkSemaphore sem;

    if(vkCreateSemaphore(attr->device, &SemaphoreCreateInfo, nullptr, &sem)!=VK_SUCCESS)
        return(nullptr);

    return(new Semaphore(attr->device,sem));
}

Queue *GPUDevice::CreateQueue(const uint32_t fence_count,const bool create_signaled)
{
    if(fence_count<=0)return(nullptr);

    Fence **fence_list=new Fence *[fence_count];

    for(uint32_t i=0;i<fence_count;i++)
        fence_list[i]=CreateFence(create_signaled);

    return(new Queue(attr->device,attr->graphics_queue,fence_list,fence_count));
}
VK_NAMESPACE_END
