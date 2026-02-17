#ifndef HGL_VULKAN_GRAPH_FENCE_INCLUDE
#define HGL_VULKAN_GRAPH_FENCE_INCLUDE

#include<hgl/vk/VK.h>
namespace hgl::graph{
class Fence
{
    VkDevice device;
    VkFence fence;

private:

    friend class VulkanDevice;

    Fence(VkDevice d,VkFence f)
    {
        device=d;
        fence=f;
    }

public:

    ~Fence();

    operator VkFence(){return fence;}
};//class Fence
}//namespace hgl::graph
#endif//HGL_VULKAN_GRAPH_FENCE_INCLUDE
