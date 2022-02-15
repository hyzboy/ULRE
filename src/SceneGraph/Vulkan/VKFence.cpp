#include<hgl/graph/VKFence.h>
VK_NAMESPACE_BEGIN
GPUFence::~GPUFence()
{
    vkDestroyFence(device,fence,nullptr);
}
VK_NAMESPACE_END
