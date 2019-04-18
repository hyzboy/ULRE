#ifndef HGL_GRAPH_RENDER_SURFACE_INCLUDE
#define HGL_GRAPH_RENDER_SURFACE_INCLUDE

#include<hgl/type/List.h>
#include"VK.h"
#include"Window.h"
#include"VKDeviceAttribute.h"

VK_NAMESPACE_BEGIN
struct PhysicalDevice;
class Buffer;
class VertexBuffer;
class CommandBuffer;
class RenderPass;

using RefDeviceAttribute=SharedPtr<DeviceAttribute>;

class Device
{
    RefDeviceAttribute rsa;

private:

    friend Device *CreateRenderDevice(VkInstance,const PhysicalDevice *,Window *);

    Device(RefDeviceAttribute &ref_rsa)
    {
        rsa=ref_rsa;
    }

public:

    virtual ~Device()=default;

            VkSurfaceKHR    GetSurface          ()      {return rsa->surface;}
            VkDevice        GetDevice           ()      {return rsa->device;}
    const   PhysicalDevice *GetPhysicalDevice   ()const {return rsa->physical_device;}
    const   VkExtent2D &    GetExtent           ()const {return rsa->swapchain_extent;}

public:

    Buffer *            CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize size,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);
    VertexBuffer *      CreateBuffer(VkBufferUsageFlags buf_usage,VkFormat format,uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);

#define CREATE_FORMAT_BUFFER_OBJECT(LargeName,type)  VertexBuffer *Create##LargeName(VkFormat format,uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,format,count,sharing_mode);}
    CREATE_FORMAT_BUFFER_OBJECT(VBO,VERTEX)
    CREATE_FORMAT_BUFFER_OBJECT(IBO,INDEX)
#undef CREATE_FORMAT_BUFFER_OBJECT

#define CREATE_BUFFER_OBJECT(LargeName,type)  Buffer *Create##LargeName(VkDeviceSize size,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size,sharing_mode);}

    CREATE_BUFFER_OBJECT(UBO,UNIFORM)
    CREATE_BUFFER_OBJECT(SBO,STORAGE)
    CREATE_BUFFER_OBJECT(INBO,INDIRECT)

#undef CREATE_BUFFER_OBJECT

    CommandBuffer *     CreateCommandBuffer ();

//    DescriptorSet *     CreateDescSet(int);

    RenderPass *CreateRenderPass();
};//class Device
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDER_SURFACE_INCLUDE
