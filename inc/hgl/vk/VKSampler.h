#ifndef HGL_GRAPH_VULKAN_SAMPLER_INCLUDE
#define HGL_GRAPH_VULKAN_SAMPLER_INCLUDE

#include<hgl/vk/VK.h>
namespace hgl::graph{
class Device;

class Sampler
{
    VkDevice device;
    VkSampler sampler;

protected:

    friend class VulkanDevice;

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
}//namespace hgl::graph
#endif//HGL_GRAPH_VULKAN_SAMPLER_INCLUDE
