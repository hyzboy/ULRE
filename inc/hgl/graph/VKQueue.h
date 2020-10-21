#ifndef HGL_GRAPH_VULKAN_SUBMIT_QUEUE_INCLUDE
#define HGL_GRAPH_VULKAN_SUBMIT_QUEUE_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKFence.h>
VK_NAMESPACE_BEGIN
class GPUQueue
{
protected:

    GPUDevice *device;
    VkQueue queue;
    
    uint32_t current_fence;
    ObjectList<GPUFence> fence_list;

    SubmitInfo submit_info;

public:

    GPUQueue(GPUDevice *dev,VkQueue q,const uint32_t fence_count=1);
    virtual ~GPUQueue();
    
    bool WaitQueue();
    bool WaitFence(const bool wait_all=true,const uint64_t time_out=HGL_NANO_SEC_PER_SEC);
    bool Submit(const VkCommandBuffer &cmd_buf,GPUSemaphore *wait_sem,GPUSemaphore *complete_sem);
    bool Submit(const VkCommandBuffer *cmd_buf,const uint32_t count,GPUSemaphore *wait_sem,GPUSemaphore *complete_sem);
};//class SumbitQueue
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SUBMIT_QUEUE_INCLUDE
