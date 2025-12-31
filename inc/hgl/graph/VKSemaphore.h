#ifndef HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE
#define HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE

#include<hgl/graph/VK.h>
VK_NAMESPACE_BEGIN
class Semaphore
{
    VkDevice device;
    VkSemaphore sem;

private:

    friend class VulkanDevice;

    Semaphore(VkDevice d,VkSemaphore s)
    {
        device=d;
        sem=s;
    }

public:

    ~Semaphore();

    operator VkSemaphore(){return sem;}

    operator const VkSemaphore *()const{return &sem;}
};//class Semaphore
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE
