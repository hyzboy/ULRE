#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<hgl/type/Smart.h>
#include<hgl/log/Log.h>
#include<cstdint>
#include<cstdlib>

namespace hgl::graph{

DeviceQueue::DeviceQueue(const VulkanDevAttr *attr,VkQueue q,Fence **fl,const uint32_t fc)
{
    dev_attr=attr;
    device=attr?attr->device:VK_NULL_HANDLE;
    queue=q;

    current_fence=0;
    last_submitted_fence=0;
    submit_count=0;
    device_lost=false;
    device_lost_submit_reported=false;
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
    if(device_lost)
        return false;

    if(queue==VK_NULL_HANDLE)
    {
        LogError("[QUEUE] WaitQueue called with null queue");
        return false;
    }

    VkResult result=vkQueueWaitIdle(queue);
    if(result!=VK_SUCCESS)
    {
        if(result==VK_ERROR_DEVICE_LOST)
            device_lost=true;
        LogError("[QUEUE] vkQueueWaitIdle failed queue=%p result=%d",
                 (void*)queue,static_cast<int>(result));
        return false;
    }

    return true;
}

VkResult DeviceQueue::Present(const VkPresentInfoKHR *pi)
{
    if(device_lost)
        return VK_ERROR_DEVICE_LOST;

    VkResult result=vkQueuePresentKHR(queue,pi);
    if(result==VK_ERROR_DEVICE_LOST)
        device_lost=true;
    return result;
}

bool DeviceQueue::WaitLastSubmitFence(const bool wait_all,uint64_t time_out)
{
    if(device_lost)
        return false;

    if(!has_last_submit)
        return(true);

    if(!fence_list||last_submitted_fence>=fence_count||!fence_list[last_submitted_fence])
    {
        LogError("[FENCE] WaitLastSubmit has invalid fence slot=%u fence_count=%u",
                 last_submitted_fence,fence_count);
        return false;
    }

    VkFence fence=*fence_list[last_submitted_fence];
    VkResult result=vkWaitForFences(device,1,&fence,wait_all,time_out);

    if(result!=VK_SUCCESS)
    {
        if(result==VK_ERROR_DEVICE_LOST)
            device_lost=true;
        LogWarning("[FENCE] WaitLastSubmit FAILED res=%d fence=%p slot=%u submit=%llu queue=%p timeout_ns=%llu",
                   static_cast<int>(result),(void*)fence,last_submitted_fence,
                   static_cast<unsigned long long>(submit_count),(void*)queue,
                   static_cast<unsigned long long>(time_out));
        return(false);
    }

    return(true);
}

bool DeviceQueue::WaitFenceAt(const uint32_t fence_index,const uint64_t time_out)
{
    if(device_lost)
        return false;

    if(!fence_list||fence_index>=fence_count||!fence_list[fence_index])
    {
        LogError("[FENCE] WaitFenceAt has invalid slot=%u fence_count=%u queue=%p",
                 fence_index,fence_count,(void*)queue);
        return false;
    }

    Fence *fence_object=fence_list[fence_index];
    if(!fence_object->queue_submitted)
        return true;

    VkFence fence=*fence_object;
    VkResult result=vkWaitForFences(device,1,&fence,VK_TRUE,time_out);
    if(result!=VK_SUCCESS)
    {
        if(result==VK_ERROR_DEVICE_LOST)
            device_lost=true;
        LogWarning("[FENCE] WaitFenceAt FAILED res=%d fence=%p slot=%u submit=%llu queue=%p timeout_ns=%llu",
                   static_cast<int>(result),(void*)fence,fence_index,
                   static_cast<unsigned long long>(submit_count),(void*)queue,
                   static_cast<unsigned long long>(time_out));
        return false;
    }

    return true;
}

bool DeviceQueue::IsLastSubmitComplete() const
{
    if(!has_last_submit || !fence_list)
        return true;

    if(last_submitted_fence>=fence_count)
    {
        GLogError("[FENCE] IsLastSubmitComplete has invalid slot=%u fence_count=%u queue=%p",
                  last_submitted_fence,fence_count,(void*)queue);
        return false;
    }

    Fence *f = fence_list[last_submitted_fence];
    if(!f)
        return true;

    VkResult result=f->GetStatus();
    if(result!=VK_SUCCESS&&result!=VK_NOT_READY)
        GLogError("[FENCE] IsLastSubmitComplete failed result=%d fence=%p slot=%u submit=%llu queue=%p",
                  static_cast<int>(result),(void*)*f,last_submitted_fence,
                  static_cast<unsigned long long>(submit_count),(void*)queue);
    return result==VK_SUCCESS;
}

bool DeviceQueue::Submit(const VkCommandBuffer *cmd_buf,const uint32_t cb_count,
                         const SemaphoreSubmit *waits,const uint32_t wait_count,
                         const SemaphoreSubmit *signals,const uint32_t signal_count)
{
    static const bool sync_trace_enabled=(getenv("VK_SYNC_TRACE")!=nullptr);

    if(device_lost)
    {
        if(!device_lost_submit_reported)
        {
            device_lost_submit_reported=true;
            LogError("[QUEUE] Submit blocked after VK_ERROR_DEVICE_LOST queue=%p submit=%llu",
                     (void*)queue,static_cast<unsigned long long>(submit_count+1));
        }
        return false;
    }

    if(!cmd_buf||cb_count==0||!queue||!device||!fence_list||fence_count==0
       ||(wait_count>0&&!waits)||(signal_count>0&&!signals))
    {
        LogError("[QUEUE] Submit rejected invalid arguments queue=%p cmd_count=%u wait_count=%u signal_count=%u fence_count=%u",
                 (void*)queue,cb_count,wait_count,signal_count,fence_count);
        return(false);
    }

    for(uint32_t i=0;i<cb_count;i++)
    {
        if(cmd_buf[i]==VK_NULL_HANDLE)
        {
            LogError("[QUEUE] Submit rejected null command buffer index=%u/%u queue=%p",
                     i,cb_count,(void*)queue);
            return false;
        }
    }

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

    if(current_fence>=fence_count||!fence_list[current_fence])
    {
        LogError("[FENCE] Submit has invalid fence slot=%u fence_count=%u queue=%p",
                 current_fence,fence_count,(void*)queue);
        return false;
    }

    Fence *fence_object=fence_list[current_fence];
    VkFence fence=*fence_object;
    if(fence==VK_NULL_HANDLE)
    {
        LogError("[FENCE] Submit has null fence handle slot=%u queue=%p",
                 current_fence,(void*)queue);
        return false;
    }

    // A ring slot can still be in use if the GPU falls behind the submission ring.
    // Wait for its previous submission before resetting the fence.
    if(fence_object->queue_submitted)
    {
        VkResult status=fence_object->GetStatus();
        if(status==VK_NOT_READY)
        {
            LogWarning("[FENCE] Submit waiting for ring-slot reuse slot=%u fence=%p next_submit=%llu queue=%p",
                       current_fence,(void*)fence,
                       static_cast<unsigned long long>(submit_count+1),(void*)queue);
            status=fence_object->Wait(HGL_NANO_SEC_PER_SEC);
        }

        if(status!=VK_SUCCESS)
        {
            if(status==VK_ERROR_DEVICE_LOST)
                device_lost=true;
            LogError("[FENCE] Submit cannot reuse fence slot=%u fence=%p result=%d next_submit=%llu queue=%p",
                     current_fence,(void*)fence,static_cast<int>(status),
                     static_cast<unsigned long long>(submit_count+1),(void*)queue);
            return false;
        }
    }

    if (fence != VK_NULL_HANDLE)
    {
        VkResult reset_res = fence_object->Reset();
        if (reset_res != VK_SUCCESS)
        {
            if(reset_res==VK_ERROR_DEVICE_LOST)
                device_lost=true;
            LogError("[FENCE] Submit reset fence FAILED res=%d fence=%p slot=%u next_submit=%llu queue=%p",
                     static_cast<int>(reset_res),(void*)fence,current_fence,
                     static_cast<unsigned long long>(submit_count+1),(void*)queue);
            return false;
        }
        fence_object->queue_submitted=false;
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
        fence_object->queue_submitted=true;
        ++submit_count;

        if(++current_fence==fence_count)
            current_fence=0;
    }
    else
    {
        if(result==VK_ERROR_DEVICE_LOST)
            device_lost=true;
        LogError("[QUEUE] vkQueueSubmit2 failed result=%d queue=%p fence=%p fence_slot=%u submit=%llu cmd_count=%u wait_count=%u signal_count=%u",
                 static_cast<int>(result),(void*)queue,(void*)fence,current_fence,
                 static_cast<unsigned long long>(submit_count+1),cb_count,real_wait_count,real_signal_count);
    }

    // Enable with VK_SYNC_TRACE=1 to capture successful submit dependencies.
    // On failure, always dump the same data so the first error remains actionable.
    if(sync_trace_enabled||result!=VK_SUCCESS)
    {
        LogInfo("[QUEUE][SYNC] submit=%llu result=%d queue=%p fence=%p fence_slot=%u cmd_count=%u wait_count=%u signal_count=%u",
                static_cast<unsigned long long>(submit_count+(result==VK_SUCCESS?0:1)),
                static_cast<int>(result),(void*)queue,(void*)fence,
                result==VK_SUCCESS?(current_fence+fence_count-1)%fence_count:current_fence,
                cb_count,real_wait_count,real_signal_count);

        for(uint32_t i=0;i<cb_count;i++)
            LogInfo("[QUEUE][SYNC]   cmd[%u]=%p",i,(void*)cmd_buf[i]);

        uint32_t wait_info_index=0;
        for(uint32_t i=0;i<wait_count;i++)
        {
            if(!waits[i].semaphore)
                continue;

            const VkSemaphoreSubmitInfo &info=wait_infos[wait_info_index++];
            LogInfo("[QUEUE][SYNC]   wait[%u] semaphore=%p type=%s value=%llu stage=0x%llx",
                    wait_info_index-1,(void*)info.semaphore,
                    waits[i].semaphore->IsTimeline()?"timeline":"binary",
                    static_cast<unsigned long long>(info.value),
                    static_cast<unsigned long long>(info.stageMask));
        }

        uint32_t signal_info_index=0;
        for(uint32_t i=0;i<signal_count;i++)
        {
            if(!signals[i].semaphore)
                continue;

            const VkSemaphoreSubmitInfo &info=signal_infos[signal_info_index++];
            LogInfo("[QUEUE][SYNC]   signal[%u] semaphore=%p type=%s value=%llu stage=0x%llx",
                    signal_info_index-1,(void*)info.semaphore,
                    signals[i].semaphore->IsTimeline()?"timeline":"binary",
                    static_cast<unsigned long long>(info.value),
                    static_cast<unsigned long long>(info.stageMask));
        }
    }

    return(result==VK_SUCCESS);
}

bool DeviceQueue::Submit(VulkanCmdBuffer *cmd_buf,
                         const SemaphoreSubmit *waits,const uint32_t wait_count,
                         const SemaphoreSubmit *signals,const uint32_t signal_count)
{
    if(!cmd_buf)
        return false;

    if(cmd_buf->IsBegin()&&!cmd_buf->End())
    {
        LogError("[QUEUE] Submit aborted: vkEndCommandBuffer failed cmd=%p queue=%p",
                 (void*)(VkCommandBuffer)*cmd_buf,(void*)queue);
        return false;
    }

    VkCommandBuffer vk_cmd=*cmd_buf;

    return Submit(&vk_cmd,1,waits,wait_count,signals,signal_count);
}
}//namespace hgl::graph
