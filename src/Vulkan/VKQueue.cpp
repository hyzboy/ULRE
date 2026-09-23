#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<hgl/type/Smart.h>
#include<hgl/log/Log.h>
#include<cstdint>

namespace hgl::graph{

DeviceQueue::DeviceQueue(const VulkanDevAttr *attr,VkQueue q,Fence **fl,const uint32_t fc)
{
    dev_attr=attr;
    device=attr?attr->device:VK_NULL_HANDLE;
    queue=q;

    current_fence=0;
    last_submitted_fence=0;
    has_last_submit=false;
    fence_list=fl;
    fence_count=fc;
}

DeviceQueue::~DeviceQueue()
{
    LogDebug("DeviceQueue::~DeviceQueue() - fence_count=%u", fence_count);
    SAFE_CLEAR_OBJECT_ARRAY_OBJECT(fence_list,fence_count)
    LogDebug("DeviceQueue::~DeviceQueue() - Complete");
}

bool DeviceQueue::WaitQueue()
{
    VkResult result=vkQueueWaitIdle(queue);
    return(result==VK_SUCCESS);
}

bool DeviceQueue::WaitFence(const bool wait_all,uint64_t time_out)
{
    return WaitLastSubmitFence(wait_all, time_out);
}

bool DeviceQueue::WaitLastSubmitFence(const bool wait_all,uint64_t time_out)
{
    if(!has_last_submit)
        return(true);

    VkFence fence=*fence_list[last_submitted_fence];
    VkResult result=vkWaitForFences(device,1,&fence,wait_all,time_out);

    if(result!=VK_SUCCESS)
    {
        LogWarning("[FENCE] WaitLastSubmit FAILED res=%d fence=%p", static_cast<int>(result), (void*)fence);
        return(false);
    }

    return(true);
}

bool DeviceQueue::Submit(const VkCommandBuffer *cmd_buf,const uint32_t cb_count,Semaphore *wait_sem,Semaphore *complete_sem)
{
    if(!cmd_buf||cb_count==0)
        return(false);

    VkSemaphoreSubmitInfo wait_sem_info{};
    if(wait_sem)
    {
        wait_sem_info.sType     =VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        wait_sem_info.pNext     =nullptr;
        wait_sem_info.semaphore =*wait_sem;
        wait_sem_info.value     =0;
        wait_sem_info.stageMask =VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        wait_sem_info.deviceIndex=0;
    }

    VkSemaphoreSubmitInfo signal_sem_info{};
    if(complete_sem)
    {
        signal_sem_info.sType     =VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal_sem_info.pNext     =nullptr;
        signal_sem_info.semaphore =*complete_sem;
        signal_sem_info.value     =0;
        signal_sem_info.stageMask =VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        signal_sem_info.deviceIndex=0;
    }

    constexpr uint32_t STACK_CB_COUNT = 8;
    VkCommandBufferSubmitInfo stack_cb_infos[STACK_CB_COUNT];
    AutoDeleteArray<VkCommandBufferSubmitInfo> heap_cb_infos;
    VkCommandBufferSubmitInfo *cb_infos = stack_cb_infos;

    if(cb_count > STACK_CB_COUNT)
    {
        cb_infos = heap_cb_infos.alloc(cb_count);
    }

    for(uint32_t i=0;i<cb_count;i++)
    {
        cb_infos[i].sType           =VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        cb_infos[i].pNext           =nullptr;
        cb_infos[i].commandBuffer   =cmd_buf[i];
        cb_infos[i].deviceMask      =0;
    }

    VkSubmitInfo2 submit_info2{};
    submit_info2.sType                      =VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info2.pNext                      =nullptr;
    submit_info2.flags                      =0;
    submit_info2.waitSemaphoreInfoCount     =wait_sem?1:0;
    submit_info2.pWaitSemaphoreInfos        =wait_sem?&wait_sem_info:nullptr;
    submit_info2.commandBufferInfoCount     =cb_count;
    submit_info2.pCommandBufferInfos        =cb_infos;
    submit_info2.signalSemaphoreInfoCount   =complete_sem?1:0;
    submit_info2.pSignalSemaphoreInfos      =complete_sem?&signal_sem_info:nullptr;

    VkFence fence=*fence_list[current_fence];

    if (fence != VK_NULL_HANDLE)
    {
        VkResult reset_res = vkResetFences(device, 1, &fence);
        if (reset_res != VK_SUCCESS)
        {
            LogWarning("[FENCE] Submit reset fence FAILED res=%d fence=%p", static_cast<int>(reset_res), (void *)fence);
        }
    }

    VkResult result;
    if(dev_attr&&dev_attr->queue_submit2)
        result=dev_attr->queue_submit2(queue,1,&submit_info2,fence);
    else
        result=vkQueueSubmit2(queue,1,&submit_info2,fence);

    if(result==VK_SUCCESS)
    {
        last_submitted_fence=current_fence;
        has_last_submit=true;

        if(++current_fence==fence_count)
            current_fence=0;
    }
    else
    {
        LogError("DeviceQueue::Submit vkQueueSubmit2 failed with result %d", static_cast<int>(result));
    }

    return(result==VK_SUCCESS);
}

bool DeviceQueue::Submit(VulkanCmdBuffer *cmd_buf,Semaphore *wait_sem,Semaphore *complete_sem)
{
    if(cmd_buf->IsBegin())
        cmd_buf->End();

    VkCommandBuffer vk_cmd=*cmd_buf;

    return Submit(&vk_cmd,1,wait_sem,complete_sem);
}
}//namespace hgl::graph

