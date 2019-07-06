#ifndef HGL_GRAPH_VULKAN_MEMORY_INCLUDE
#define HGL_GRAPH_VULKAN_MEMORY_INCLUDE

#include<hgl/graph/vulkan/VK.h>
VK_NAMESPACE_BEGIN
class Memory
{
    VkDevice                device;
    VkDeviceMemory          memory;
    VkMemoryRequirements    req;
    uint32_t                index;
    uint32_t                properties;

private:

    friend Memory *CreateMemory(VkDevice device,const PhysicalDevice *pd,const VkMemoryRequirements &req,uint32_t properties);

    Memory(VkDevice dev,VkDeviceMemory dm,const VkMemoryRequirements &mr,const uint32 i,const uint32_t p)
    {
        device=dev;
        memory=dm;
        req=mr;
        index=i;
        properties=p;
    }

public:

    virtual ~Memory();

    operator VkDeviceMemory(){return memory;}

    const VkMemoryRequirements &GetRequirements ()const{return req;}

    const uint32_t              GetType         ()const{return req.memoryTypeBits;}
    const VkDeviceSize          GetSize         ()const{return req.size;}
    const VkDeviceSize          GetAligment     ()const{return req.alignment;}
    const uint32_t              GetTypeIndex    ()const{return index;}
    const uint32_t              GetProperties   ()const{return properties;}
    
    void *Map();
    void *Map(VkDeviceSize offset,VkDeviceSize size);
    void Unmap();

    bool Write(const void *ptr,VkDeviceSize start,VkDeviceSize size);
    bool Write(const void *ptr){return Write(ptr,0,req.size);}

    bool BindBuffer(VkBuffer buffer);
    bool BindImage(VkImage image);
};//class Memory

Memory *CreateMemory(VkDevice device,const PhysicalDevice *pd,const VkMemoryRequirements &req,uint32_t properties);
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_MEMORY_INCLUDE
