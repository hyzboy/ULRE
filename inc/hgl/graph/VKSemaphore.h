#ifndef HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE
#define HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE

#include<hgl/graph/VK.h>
VK_NAMESPACE_BEGIN
class GPUSemaphore
{
    VkDevice device;
    VkSemaphore sem;

private:

    friend class Device;

    GPUSemaphore(VkDevice d,VkSemaphore s)
    {
        device=d;
        sem=s;
    }

public:

    ~GPUSemaphore();

    operator VkSemaphore(){return sem;}

    operator const VkSemaphore *()const{return &sem;}
};//class GPUSemaphore
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE
