#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/log/Log.h>
#include<cstdint>
#include<chrono>
#include<thread>

namespace hgl::graph{
namespace
{
    const VkPipelineStageFlags pipe_stage_flags=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
}//namespace

DeviceQueue::DeviceQueue(VkDevice dev,VkQueue q,Fence **fl,const uint32_t fc)
{
    device=dev;
    queue=q;

    current_fence=0;
    last_submitted_fence=0;
    has_last_submit=false;
    fence_list=fl;
    fence_count=fc;

    last_submit_result=VK_SUCCESS;
    last_present_result=VK_SUCCESS;
    device_lost=false;

    submit_info.pWaitDstStageMask       = &pipe_stage_flags;
}

DeviceQueue::~DeviceQueue()
{
    LogDebug("DeviceQueue::~DeviceQueue() - fence_count=%u", fence_count);
    // Note: VkQueue is retrieved via vkGetDeviceQueue and is implicitly destroyed
    // when VkDevice is destroyed. Multiple DeviceQueue instances may share the same
    // VkQueue handle, so we should NOT untrack it here.
    // The VulkanDevice will handle cleanup of the actual queue.

    SAFE_CLEAR_OBJECT_ARRAY_OBJECT(fence_list,fence_count)
    LogDebug("DeviceQueue::~DeviceQueue() - Complete");
}

bool DeviceQueue::WaitQueue()
{
    auto start = std::chrono::high_resolution_clock::now();
    LogInfo("[FENCE] WaitQueue START queue=%p", (void*)queue);

    VkResult result=vkQueueWaitIdle(queue);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    LogInfo("[FENCE] WaitQueue END result=%d time=%lldms", static_cast<int>(result), duration);

    if(result!=VK_SUCCESS)
        return(false);

    return(true);
}

bool DeviceQueue::WaitFence(const bool wait_all,uint64_t time_out)
{
    return WaitLastSubmitFence(wait_all, time_out);
}

bool DeviceQueue::WaitLastSubmitFence(const bool wait_all,uint64_t time_out)
{
    if(device_lost)
    {
        has_last_submit=false;
        LogWarning("[FENCE] WaitLastSubmit SKIP (device lost)");
        return false;
    }

    if(!has_last_submit)
        return(true);

    VkFence fence=*fence_list[last_submitted_fence];
    auto start = std::chrono::high_resolution_clock::now();
    LogInfo("[FENCE] WaitLastSubmit START fence=%p last_fence=%u", (void*)fence, last_submitted_fence);

    const auto timeout_ns = static_cast<uint64_t>(time_out);
    const auto deadline = start + std::chrono::nanoseconds(timeout_ns);
    VkResult result = VK_NOT_READY;

    // Use polling instead of vkWaitForFences to avoid rare tool/driver deadlocks
    // observed during RenderDoc frame capture.
    do
    {
        result = vkGetFenceStatus(device, fence);

        if (result == VK_SUCCESS)
        {
            has_last_submit = false;

            auto after_wait = std::chrono::high_resolution_clock::now();
            auto wait_duration = std::chrono::duration_cast<std::chrono::milliseconds>(after_wait - start).count();
            LogInfo("[FENCE] WaitLastSubmit END result=%d time=%lldms", static_cast<int>(result), wait_duration);
            return true;
        }

        if (result != VK_NOT_READY)
        {
            has_last_submit = false;

            auto after_wait = std::chrono::high_resolution_clock::now();
            auto wait_duration = std::chrono::duration_cast<std::chrono::milliseconds>(after_wait - start).count();
            LogWarning("[FENCE] WaitLastSubmit FAILED res=%d fence=%p time=%lldms", static_cast<int>(result), (void*)fence, wait_duration);
            return false;
        }

        if (std::chrono::high_resolution_clock::now() >= deadline)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (true);

    auto after_wait = std::chrono::high_resolution_clock::now();
    auto wait_duration = std::chrono::duration_cast<std::chrono::milliseconds>(after_wait - start).count();
    LogWarning("[FENCE] WaitLastSubmit TIMEOUT fence=%p timeout_ns=%llu time=%lldms wait_all=%d", (void*)fence, static_cast<unsigned long long>(timeout_ns), wait_duration, wait_all ? 1 : 0);
    return false;
}

bool DeviceQueue::Submit(const VkCommandBuffer *cmd_buf,const uint32_t cb_count,Semaphore *wait_sem,Semaphore *complete_sem)
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

    // wait 信号的意思是等待这个Image有效
    // signal 则是这个queue已执行完成，和fence功能类似。
    // 所以Wait信号一般是上一次的signal信号

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
    auto submit_start = std::chrono::high_resolution_clock::now();
    LogInfo("[FENCE] Submit START fence=%p current_fence=%u", (void*)fence, current_fence);

    if (fence != VK_NULL_HANDLE)
    {
        VkResult reset_res = vkResetFences(device, 1, &fence);
        LogInfo("[FENCE] Submit reset fence result=%d", static_cast<int>(reset_res));
        if (reset_res != VK_SUCCESS)
        {
            GLogWarning("[FENCE] Submit reset fence FAILED res=%d fence=%p", static_cast<int>(reset_res), (void *)fence);
        }
    }

    VkResult result=vkQueueSubmit(queue,1,&submit_info,fence);
    last_submit_result=result;

    if(result==VK_ERROR_DEVICE_LOST)
    {
        device_lost=true;
        has_last_submit=false;
    }

    auto submit_end = std::chrono::high_resolution_clock::now();
    auto submit_duration = std::chrono::duration_cast<std::chrono::milliseconds>(submit_end - submit_start).count();
    LogInfo("[FENCE] Submit END result=%d time=%lldms", static_cast<int>(result), submit_duration);

    if(result==VK_SUCCESS)
    {
        last_submitted_fence=current_fence;
        has_last_submit=true;

        if(++current_fence==fence_count)
            current_fence=0;
    }

    //不在这里立即等待fence完成。等待操作放在下一帧开始前，确保上一帧完成后再复用该fence。

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
