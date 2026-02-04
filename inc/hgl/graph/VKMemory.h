#ifndef HGL_GRAPH_VULKAN_MEMORY_INCLUDE
#define HGL_GRAPH_VULKAN_MEMORY_INCLUDE

#include<hgl/graph/VK.h>
VK_NAMESPACE_BEGIN

/**
 * Memory usage patterns for Vulkan buffers
 */
enum class MemoryUsage
{
    CPUOnly,        // HOST_VISIBLE | HOST_COHERENT (current default)
    GPUOnly,        // DEVICE_LOCAL (best performance)
    CPUToGPU,       // Staging: HOST_VISIBLE | HOST_COHERENT
    GPUToCPU,       // Readback: HOST_VISIBLE | HOST_CACHED
    ReBAR           // HOST_VISIBLE | HOST_COHERENT | DEVICE_LOCAL (requires Resizable BAR)
};

class DeviceMemory
{
    VkDevice                device;
    VkDeviceMemory          memory;
    VkMemoryRequirements    req;
    uint32_t                index;
    uint32_t                properties;

    VkMappedMemoryRange     memory_range;
    VkDeviceSize            nonCoherentAtomSize;

private:

    friend class VulkanDevice;

    DeviceMemory(VkDevice dev,VkDeviceMemory dm,const VkMemoryRequirements &mr,const uint32 i,const uint32_t p,const VkDeviceSize cas);

public:

    virtual ~DeviceMemory();

    operator VkDeviceMemory(){return memory;}

    const VkMemoryRequirements &GetRequirements ()const{return req;}

    const uint32_t              GetType         ()const{return req.memoryTypeBits;}
    const VkDeviceSize          GetSize         ()const{return req.size;}
    const VkDeviceSize          GetAlignment    ()const{return req.alignment;}
    const uint32_t              GetTypeIndex    ()const{return index;}
    const uint32_t              GetProperties   ()const{return properties;}

    void *  Map         ();
    void *  Map         (VkDeviceSize offset,VkDeviceSize size);
    void    Unmap       ();

    bool    Write       (const void *ptr,VkDeviceSize start,  VkDeviceSize size);
    bool    Write       (const void *ptr,                     VkDeviceSize size)  {return Write(ptr,0,size);}
    bool    Write       (const void *ptr)                                         {return Write(ptr,0,req.size);}

    bool    BindBuffer  (VkBuffer buffer);
    bool    BindImage   (VkImage image);

    void    Flush       (VkDeviceSize,VkDeviceSize);
    void    Flush       (VkDeviceSize size){Flush(0,size);}
};//class DeviceMemory
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MEMORY_INCLUDE
