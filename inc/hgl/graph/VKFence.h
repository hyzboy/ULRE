#ifndef HGL_VULKAN_GRAPH_FENCE_INCLUDE
#define HGL_VULKAN_GRAPH_FENCE_INCLUDE

#include<hgl/graph/VK.h>
VK_NAMESPACE_BEGIN
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
VK_NAMESPACE_END
#endif//HGL_VULKAN_GRAPH_FENCE_INCLUDE
