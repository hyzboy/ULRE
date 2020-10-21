#ifndef HGL_GRAPH_VULKAN_SAMPLER_INCLUDE
#define HGL_GRAPH_VULKAN_SAMPLER_INCLUDE

#include<hgl/graph/VK.h>
VK_NAMESPACE_BEGIN
class Device;

class Sampler
{
    VkDevice device;
    VkSampler sampler;

protected:

    friend class RenderDevice;

    Sampler(VkDevice dev,VkSampler s)
    {
        device=dev;
        sampler=s;
    }

public:

    ~Sampler();

    operator VkSampler(){return sampler;}
    operator const VkSampler()const{return sampler;}
};//class Sampler
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SAMPLER_INCLUDE
