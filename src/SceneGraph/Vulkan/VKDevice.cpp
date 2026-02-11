#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSemaphore.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKBufferUpdateQueue.h>
#include<hgl/graph/VKComputePipeline.h>

VK_NAMESPACE_BEGIN
VulkanDevice::VulkanDevice(VulkanDevAttr *da)
{
    attr=da;
    buffer_update_queue = new BufferUpdateQueue(da->device);
}

VulkanDevice::~VulkanDevice()
{
    delete buffer_update_queue;
    delete attr;
}

VkCommandBuffer VulkanDevice::CreateCommandBuffer(const AnsiString &name)
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

RenderCmdBuffer *VulkanDevice::CreateRenderCommandBuffer(const AnsiString &name)
{
    VkCommandBuffer cb=CreateCommandBuffer(name);

    if(cb==VK_NULL_HANDLE)return(nullptr);

    return(new RenderCmdBuffer(attr,cb));
}

TextureCmdBuffer *VulkanDevice::CreateTextureCommandBuffer(const AnsiString &name)
{
    VkCommandBuffer cb=CreateCommandBuffer(name);

    if(cb==VK_NULL_HANDLE)return(nullptr);

    return(new TextureCmdBuffer(attr,cb));
}

/**
 * 创建栅栏
 * @param create_signaled 是否创建初始信号
 */
Fence *VulkanDevice::CreateFence(bool create_signaled)
{
    FenceCreateInfo fenceInfo(create_signaled?VK_FENCE_CREATE_SIGNALED_BIT:0);

    VkFence fence;

    if(vkCreateFence(attr->device, &fenceInfo, nullptr, &fence)!=VK_SUCCESS)
        return(nullptr);

    return(new Fence(attr->device,fence));
}

Semaphore *VulkanDevice::CreateGPUSemaphore()
{
    SemaphoreCreateInfo SemaphoreCreateInfo;

    VkSemaphore sem;

    if(vkCreateSemaphore(attr->device, &SemaphoreCreateInfo, nullptr, &sem)!=VK_SUCCESS)
        return(nullptr);

    return(new Semaphore(attr->device,sem));
}

DeviceQueue *VulkanDevice::CreateQueue(const uint32_t fence_count,const bool create_signaled)
{
    if(fence_count<=0)return(nullptr);

    Fence **fence_list=new Fence *[fence_count];

    for(uint32_t i=0;i<fence_count;i++)
        fence_list[i]=CreateFence(create_signaled);

    return(new DeviceQueue(attr->device,attr->graphics_queue,fence_list,fence_count));
}

ComputePipeline *VulkanDevice::CreateComputePipeline(const AnsiString &name, VkShaderModule shader_module, VkPipelineLayout pipeline_layout)
{
    VkComputePipelineCreateInfo compute_pipeline_info = {};
    compute_pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_info.layout = pipeline_layout;
    compute_pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_pipeline_info.stage.module = shader_module;
    compute_pipeline_info.stage.pName = "main";

    VkPipeline pipeline;
    if (vkCreateComputePipelines(attr->device, attr->pipeline_cache, 1, &compute_pipeline_info, nullptr, &pipeline) != VK_SUCCESS)
    {
        LOG_ERROR(name + " create compute pipeline failed!");
        return nullptr;
    }

#ifdef _DEBUG
    if(attr->debug_utils)
        attr->debug_utils->SetPipeline(pipeline, name);
#endif//_DEBUG

    return new ComputePipeline(name, attr->device, pipeline, pipeline_layout);
}

VK_NAMESPACE_END
