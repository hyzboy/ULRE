#ifndef HGL_VULKAN_GRAPH_FENCE_INCLUDE
#define HGL_VULKAN_GRAPH_FENCE_INCLUDE

#include<hgl/graph/VK.h>
VK_NAMESPACE_BEGIN
class GPUFence
{
    VkDevice device;
    VkFence fence;

private:

    friend class GPUDevice;

    GPUFence(VkDevice d,VkFence f)
    {
        device=d;
        fence=f;
    }

public:

    ~GPUFence();

    operator VkFence(){return fence;}
};//class GPUFence
VK_NAMESPACE_END
#endif//HGL_VULKAN_GRAPH_FENCE_INCLUDE
