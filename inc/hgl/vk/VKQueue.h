#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKFence.h>
#include<hgl/time/TimeConst.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
class DeviceQueue
{
    OBJECT_LOGGER

protected:

    VkDevice device;
    VkQueue queue;

    uint32_t current_fence;
    uint32_t last_submitted_fence;
    bool has_last_submit;
    Fence **fence_list;
    uint32_t fence_count;

    SubmitInfo submit_info;

    VkResult last_submit_result;
    VkResult last_present_result;
    bool device_lost;

private:

    friend class VulkanDevice;

    DeviceQueue(VkDevice dev,VkQueue q,Fence **,const uint32_t fc);

public:

    virtual ~DeviceQueue();

    operator VkQueue(){return queue;}

    VkResult Present(const VkPresentInfoKHR *pi)
    {
        VkResult result=vkQueuePresentKHR(queue,pi);
        last_present_result=result;

        if(result==VK_ERROR_DEVICE_LOST)
        {
            device_lost=true;
            has_last_submit=false;
        }

        return result;
    }

    VkResult GetLastSubmitResult()const{return last_submit_result;}
    VkResult GetLastPresentResult()const{return last_present_result;}
    bool IsDeviceLost()const{return device_lost;}

    /**
    * 等待Submit的队列完成操作。这个操作会阻塞当前线程，所以在Submit后请不要立即使用它。而是在下一次队列提交前再做这个操作。
    */
    bool WaitQueue();

    /**
    * 等待Queue命令执行完成的fence信号
    */
    bool WaitFence(const bool wait_all=true,const uint64_t time_out=HGL_NANO_SEC_PER_SEC);

    /**
    * 等待上一条提交使用的fence信号
    */
    bool WaitLastSubmitFence(const bool wait_all=true,const uint64_t time_out=HGL_NANO_SEC_PER_SEC);

    // Clear pending submit-fence tracking (used by fatal error paths, e.g. device-lost present).
    void ClearLastSubmitFenceState(){ has_last_submit = false; }

    bool Submit(const VkCommandBuffer *cmd_buf,const uint32_t count,Semaphore *wait_sem,Semaphore *complete_sem);
    bool Submit(VulkanCmdBuffer *cmd_buf,Semaphore *wait_sem,Semaphore *complete_sem);
};//class DeviceQueue
}//namespace hgl::graph
