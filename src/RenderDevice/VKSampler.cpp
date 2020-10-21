#include<hgl/graph/VKSampler.h>
VK_NAMESPACE_BEGIN
Sampler::~Sampler()
{
    vkDestroySampler(device,sampler,nullptr);
}
VK_NAMESPACE_END
