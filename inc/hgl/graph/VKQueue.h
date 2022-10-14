#ifndef HGL_GRAPH_VULKAN_SUBMIT_QUEUE_INCLUDE
#define HGL_GRAPH_VULKAN_SUBMIT_QUEUE_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKFence.h>
VK_NAMESPACE_BEGIN
class Queue
{
protected:

    VkDevice device;
    VkQueue queue;
    
    uint32_t current_fence;
    Fence **fence_list;
    uint32_t fence_count;

    SubmitInfo submit_info;

private:

    friend class GPUDevice;

    Queue(VkDevice dev,VkQueue q,Fence **,const uint32_t fc);

public:

    virtual ~Queue();

    operator VkQueue(){return queue;}
    
    VkResult Present(const VkPresentInfoKHR *pi){return vkQueuePresentKHR(queue,pi);}

    bool WaitQueue();
    bool WaitFence(const bool wait_all=true,const uint64_t time_out=HGL_NANO_SEC_PER_SEC);
    bool Submit(const VkCommandBuffer &cmd_buf,Semaphore *wait_sem,Semaphore *complete_sem);
    bool Submit(const VkCommandBuffer *cmd_buf,const uint32_t count,Semaphore *wait_sem,Semaphore *complete_sem);
};//class SumbitQueue
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SUBMIT_QUEUE_INCLUDE
