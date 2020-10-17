#ifndef HGL_GRAPH_VULKAN_SUBMIT_QUEUE_INCLUDE
#define HGL_GRAPH_VULKAN_SUBMIT_QUEUE_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKFence.h>
VK_NAMESPACE_BEGIN
class SubmitQueue
{
protected:

    Device *device;
    VkQueue queue;
    
    uint32_t current_fence;
    ObjectList<Fence> fence_list;

    SubmitInfo submit_info;

public:

    SubmitQueue(Device *dev,VkQueue q,const uint32_t fence_count=1);
    virtual ~SubmitQueue();
    
    bool QueueWaitIdle();
    bool Wait(const bool wait_wall=true,const uint64_t time_out=HGL_NANO_SEC_PER_SEC);
    bool Submit(const VkCommandBuffer &cmd_buf,vulkan::Semaphore *wait_sem,vulkan::Semaphore *complete_sem);
    bool Submit(const VkCommandBuffer *cmd_buf,const uint32_t count,vulkan::Semaphore *wait_sem,vulkan::Semaphore *complete_sem);
};//class SumbitQueue
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SUBMIT_QUEUE_INCLUDE
