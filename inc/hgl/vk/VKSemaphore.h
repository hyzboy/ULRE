#ifndef HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE
#define HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE

#include<hgl/vk/VK.h>
namespace hgl::graph{
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
}//namespace hgl::graph
#endif//HGL_GRAPH_VULKAN_SEMAPHORE_INCLUDE
