#ifndef HGL_GRAPH_VULKAN_MEMORY_ALLOCATOR_INCLUDE
#define HGL_GRAPH_VULKAN_MEMORY_ALLOCATOR_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/MemoryAllocator.h>

VK_NAMESPACE_BEGIN
class VKMemoryAllocator:public AbstractMemoryAllocator
{
    GPUDevice *device;

    uint32_t buffer_usage_flag_bits;

    DeviceBuffer *gpu_buffer;

    VkDeviceSize range;         //ubo之类需要一个一次访问范围

protected:

    bool AllocMemory() override;

public:

    const bool      CanRealloc              ()const override{return false;}

    const uint32_t  GetBufferUsageFlagBits  ()const{return buffer_usage_flag_bits;}

    DeviceBuffer *     GetBuffer               (){return gpu_buffer;}

public:

    VKMemoryAllocator(GPUDevice *,const uint32_t flags,const VkDeviceSize r);
    ~VKMemoryAllocator();
    
    void Free() override {/* DON'T RUN ANY OPERATION.*/}

    void Flush(const VkDeviceSize);
};//class VKMemoryAllocator:public AbstractMemoryAllocator
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MEMORY_ALLOCATOR_INCLUDE
