#ifndef HGL_GRAPH_VULKAN_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_BUFFER_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class Buffer
{
    VkDevice device;
    VkBuffer buf;
    VkDeviceMemory mem;
    VkDescriptorBufferInfo buffer_info;

private:

    friend class RenderSurface;

    Buffer(VkDevice d,VkBuffer b,VkDeviceMemory dm,VkDescriptorBufferInfo dbi)
    {
        device=d;
        buf=b;
        mem=dm;
        buffer_info=dbi;
    }

public:

    virtual ~Buffer();

    uint8_t *Map();
    void Unmap();
};//class Buffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_BUFFER_INCLUDE
