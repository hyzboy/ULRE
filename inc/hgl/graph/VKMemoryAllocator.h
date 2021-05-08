#ifndef HGL_GRAPH_VULKAN_MEMORY_ALLOCATOR_INCLUDE
#define HGL_GRAPH_VULKAN_MEMORY_ALLOCATOR_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/MemoryAllocator.h>

VK_NAMESPACE_BEGIN
class VKMemoryAllocator:public AbstractMemoryAllocator
{
    GPUDevice *device;

    uint32_t buffer_usage_flag_bits;

    GPUBuffer *gpu_buffer;

protected:

    bool AllocMemory() override;

public:

    const bool      CanRealloc              ()const override{return false;}

    const uint32_t  GetBufferUsageFlagBits  ()const{return buffer_usage_flag_bits;}

public:

    VKMemoryAllocator(GPUDevice *,const uint32_t flags);
    ~VKMemoryAllocator();
    
    void Free() override {/* DON'T RUN ANY OPERATION.*/}
};//class VKMemoryAllocator:public AbstractMemoryAllocator
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MEMORY_ALLOCATOR_INCLUDE
