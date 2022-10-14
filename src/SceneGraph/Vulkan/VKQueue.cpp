#include<hgl/graph/VKQueue.h>
#include<hgl/graph/VKSemaphore.h>

VK_NAMESPACE_BEGIN
namespace 
{
    const VkPipelineStageFlags pipe_stage_flags=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
}//namespace

GPUQueue::GPUQueue(VkDevice dev,VkQueue q,Fence **fl,const uint32_t fc)
{
    device=dev;
    queue=q;

    current_fence=0;
    fence_list=fl;
    fence_count=fc;

    submit_info.pWaitDstStageMask       = &pipe_stage_flags;
}

GPUQueue::~GPUQueue()
{
    SAFE_CLEAR_OBJECT_ARRAY(fence_list,fence_count)
}

bool GPUQueue::WaitQueue()
{
    VkResult result=vkQueueWaitIdle(queue);
    
    if(result!=VK_SUCCESS)
        return(false);

    return(true);
}

bool GPUQueue::WaitFence(const bool wait_all,uint64_t time_out)
{
    VkResult result;
    VkFence fence=*fence_list[current_fence];

    result=vkWaitForFences(device,1,&fence,wait_all,time_out);
    result=vkResetFences(device,1,&fence);

    if(++current_fence==fence_count)
        current_fence=0;

    return(true);
}

bool GPUQueue::Submit(const VkCommandBuffer *cmd_buf,const uint32_t cb_count,GPUSemaphore *wait_sem,GPUSemaphore *complete_sem)
{
    VkSemaphore ws;
    VkSemaphore cs;

    if(wait_sem)
    {
        ws=*wait_sem;

        submit_info.waitSemaphoreCount  =1;
        submit_info.pWaitSemaphores     =&ws;
    }
    else
    {
        submit_info.waitSemaphoreCount  =0;
        submit_info.pWaitSemaphores     =nullptr;
    }

    if(complete_sem)
    {
        cs=*complete_sem;

        submit_info.signalSemaphoreCount=1;
        submit_info.pSignalSemaphores   =&cs;
    }
    else
    {
        submit_info.signalSemaphoreCount=0;
        submit_info.pSignalSemaphores   =nullptr;
    }

    submit_info.commandBufferCount  =cb_count;
    submit_info.pCommandBuffers     =cmd_buf;
    
    VkFence fence=*fence_list[current_fence];

    VkResult result=vkQueueSubmit(queue,1,&submit_info,fence);

    //不在这里立即等待fence完成，是因为有可能queue submit需要久一点工作时间，我们这个时间可以去干别的。等在AcquireNextImage时再去等待fence，而且是另一帧的fence。这样有利于异步处理

    return(result==VK_SUCCESS);
}

bool GPUQueue::Submit(const VkCommandBuffer &cmd_buf,GPUSemaphore *wait_sem,GPUSemaphore *complete_sem)
{
    return Submit(&cmd_buf,1,wait_sem,complete_sem);
}
VK_NAMESPACE_END
