#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKFence.h>
#include<hgl/time/TimeConst.h>

VK_NAMESPACE_BEGIN
class DeviceQueue
{
protected:

    VkDevice device;
    VkQueue queue;
    
    uint32_t current_fence;
    Fence **fence_list;
    uint32_t fence_count;

    SubmitInfo submit_info;

private:

    friend class VulkanDevice;

    DeviceQueue(VkDevice dev,VkQueue q,Fence **,const uint32_t fc);

public:

    virtual ~DeviceQueue();

    operator VkQueue(){return queue;}
    
    VkResult Present(const VkPresentInfoKHR *pi){return vkQueuePresentKHR(queue,pi);}

    /**
    * 等待Submit的队列完成操作。这个操作会阻塞当前线程，所以在Submit后请不要立即使用它。而是在下一次队列提交前再做这个操作。
    */
    bool WaitQueue();

    /**
    * 等待Queue命令执行完成的fence信号
    */
    bool WaitFence(const bool wait_all=true,const uint64_t time_out=HGL_NANO_SEC_PER_SEC);

    bool Submit(const VkCommandBuffer *cmd_buf,const uint32_t count,Semaphore *wait_sem,Semaphore *complete_sem);
    bool Submit(VulkanCmdBuffer *cmd_buf,Semaphore *wait_sem,Semaphore *complete_sem);
};//class DeviceQueue
VK_NAMESPACE_END
