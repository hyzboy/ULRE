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

bool DeviceQueue::IsLastSubmitComplete() const
{
    if(!has_last_submit || !fence_list)
        return true;

    Fence *f = fence_list[last_submitted_fence];
    if(!f)
        return true;

    return f->GetStatus() == VK_SUCCESS;
}

bool DeviceQueue::Submit(const VkCommandBuffer *cmd_buf,const uint32_t cb_count,
                         const SemaphoreSubmit *waits,const uint32_t wait_count,
                         const SemaphoreSubmit *signals,const uint32_t signal_count)
{
    if(!cmd_buf||cb_count==0)
        return(false);

    constexpr uint32_t STACK_SEM_COUNT = 16;

    VkSemaphoreSubmitInfo stack_wait_infos[STACK_SEM_COUNT];
    VkSemaphoreSubmitInfo stack_signal_infos[STACK_SEM_COUNT];
    AutoDeleteArray<VkSemaphoreSubmitInfo> heap_wait_infos;
    AutoDeleteArray<VkSemaphoreSubmitInfo> heap_signal_infos;

    VkSemaphoreSubmitInfo *wait_infos = stack_wait_infos;
    if(wait_count > STACK_SEM_COUNT)
        wait_infos = heap_wait_infos.alloc(wait_count);

    // 等待列表：逐项拷贝（按信号量类型决定 value —— 二进制必须 0）
    uint32_t real_wait_count = 0;
    for(uint32_t i=0;i<wait_count;i++)
    {
        if(!waits[i].semaphore)
            continue;

        VkSemaphoreSubmitInfo &info = wait_infos[real_wait_count++];

        info.sType      =VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        info.pNext      =nullptr;
        info.semaphore  =*waits[i].semaphore;
        info.value      =waits[i].semaphore->IsTimeline()?waits[i].value:0;
        info.stageMask  =waits[i].stage_mask;
        info.deviceIndex=0;
    }

    VkSemaphoreSubmitInfo *signal_infos = stack_signal_infos;
    if(signal_count > STACK_SEM_COUNT)
        signal_infos = heap_signal_infos.alloc(signal_count);

    // 信号列表（A1：离屏提交恒 signal，主帧提交等它）
    uint32_t real_signal_count = 0;
    for(uint32_t i=0;i<signal_count;i++)
    {
        if(!signals[i].semaphore)
            continue;

        VkSemaphoreSubmitInfo &info = signal_infos[real_signal_count++];

        info.sType      =VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        info.pNext      =nullptr;
        info.semaphore  =*signals[i].semaphore;
        info.value      =signals[i].semaphore->IsTimeline()?signals[i].value:0;
        info.stageMask  =signals[i].stage_mask;
        info.deviceIndex=0;
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
    submit_info2.waitSemaphoreInfoCount     =real_wait_count;
    submit_info2.pWaitSemaphoreInfos        =real_wait_count>0?wait_infos:nullptr;
    submit_info2.commandBufferInfoCount     =cb_count;
    submit_info2.pCommandBufferInfos        =cb_infos;
    submit_info2.signalSemaphoreInfoCount   =real_signal_count;
    submit_info2.pSignalSemaphoreInfos      =real_signal_count>0?signal_infos:nullptr;

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

bool DeviceQueue::Submit(VulkanCmdBuffer *cmd_buf,
                         const SemaphoreSubmit *waits,const uint32_t wait_count,
                         const SemaphoreSubmit *signals,const uint32_t signal_count)
{
    if(!cmd_buf)
        return false;

    if(cmd_buf->IsBegin())
        cmd_buf->End();

    VkCommandBuffer vk_cmd=*cmd_buf;

    return Submit(&vk_cmd,1,waits,wait_count,signals,signal_count);
}
}//namespace hgl::graph

