#include<hgl/graph/vulkan/VKFence.h>
VK_NAMESPACE_BEGIN
Fence::~Fence()
{
    vkDestroyFence(device,fence,nullptr);
}
VK_NAMESPACE_END
