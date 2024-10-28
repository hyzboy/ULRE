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
#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/module/SwapchainModule.h>

VK_NAMESPACE_BEGIN

GraphModuleManager *InitGraphModuleManager(GPUDevice *dev);
bool ClearGraphModuleManager(GPUDevice *dev);

GPUDevice::GPUDevice(GPUDeviceAttribute *da)
{
    attr=da;

    texture_queue=nullptr;
    texture_cmd_buf=nullptr;

    InitRenderPassManage();

    sc_rt=nullptr;
    Resize(attr->surface_caps.currentExtent);

    texture_cmd_buf=CreateTextureCommandBuffer(attr->physical_device->GetDeviceName()+AnsiString(":TexCmdBuffer"));
    texture_queue=CreateQueue();
}

GPUDevice::~GPUDevice()
{
    ClearRenderPassManage();

    SAFE_CLEAR(sc_rt);

    SAFE_CLEAR(texture_queue);
    SAFE_CLEAR(texture_cmd_buf);

    delete attr;
    
    //按设计，上面那些rt/queue/cmdbuf都需要走graph_module_manager释放和申请
    ClearGraphModuleManager(this);
}

bool GPUDevice::Resize(const VkExtent2D &extent)
{
    graph_module_manager->OnResize(extent);

    SAFE_CLEAR(sc_rt);

    attr->RefreshSurfaceCaps();

    sc_rt=CreateSwapchainRenderTarget();

    return(sc_rt);
}

VkCommandBuffer GPUDevice::CreateCommandBuffer(const AnsiString &name)
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

#ifdef _DEBUG
    if(attr->debug_utils)
        attr->debug_utils->SetCommandBuffer(cmd_buf,name);
#endif//_DEBUG

    return cmd_buf;
}

RenderCmdBuffer *GPUDevice::CreateRenderCommandBuffer(const AnsiString &name)
{
    VkCommandBuffer cb=CreateCommandBuffer(name);

    if(cb==VK_NULL_HANDLE)return(nullptr);

    return(new RenderCmdBuffer(attr,cb));
}

TextureCmdBuffer *GPUDevice::CreateTextureCommandBuffer(const AnsiString &name)
{
    VkCommandBuffer cb=CreateCommandBuffer(name);

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

DeviceQueue *GPUDevice::CreateQueue(const uint32_t fence_count,const bool create_signaled)
{
    if(fence_count<=0)return(nullptr);

    Fence **fence_list=new Fence *[fence_count];

    for(uint32_t i=0;i<fence_count;i++)
        fence_list[i]=CreateFence(create_signaled);

    return(new DeviceQueue(attr->device,attr->graphics_queue,fence_list,fence_count));
}
VK_NAMESPACE_END
