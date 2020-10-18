#include<hgl/graph/vulkan/VKSemaphore.h>
VK_NAMESPACE_BEGIN
GPUSemaphore::~GPUSemaphore()
{
    vkDestroySemaphore(device,sem,nullptr);
}
VK_NAMESPACE_END
