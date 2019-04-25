#ifndef HGL_GRAPH_RENDER_SURFACE_INCLUDE
#define HGL_GRAPH_RENDER_SURFACE_INCLUDE

#include<hgl/type/List.h>
#include"VK.h"
#include"Window.h"
#include"VKDeviceAttribute.h"
#include"VKFramebuffer.h"

VK_NAMESPACE_BEGIN
struct PhysicalDevice;
class Buffer;
class VertexBuffer;
class IndexBuffer;
class CommandBuffer;
class RenderPass;
class Fence;
class Semaphore;
class Material;
class Shader;

#define MAX_FRAMES_IN_FLIGHT 2

class Device
{
    DeviceAttribute *attr;

    Semaphore *image_acquired_semaphore;
    Fence *draw_fence;

    RenderPass *main_rp;
    ObjectList<Framebuffer> main_fb;
    
    uint32_t current_frame;

    VkPresentInfoKHR present;

private:

    friend Device *CreateRenderDevice(VkInstance,const PhysicalDevice *,Window *);

    Device(DeviceAttribute *da);

public:

    virtual ~Device();

    operator VkDevice                           ()      {return attr->device;}

            VkSurfaceKHR    GetSurface          ()      {return attr->surface;}
            VkDevice        GetDevice           ()      {return attr->device;}
    const   PhysicalDevice *GetPhysicalDevice   ()const {return attr->physical_device;}
    const   VkExtent2D &    GetExtent           ()const {return attr->swapchain_extent;}

    VkDescriptorPool        GetDescriptorPool   ()      {return attr->desc_pool;}
    VkPipelineCache         GetPipelineCache    ()      {return attr->pipeline_cache;}

public:

    const   uint32_t        GetSwapChainImageCount  ()const     {return attr->sc_image_views.GetCount();}
            ImageView      *GetColorImageView       (int index) {return attr->sc_image_views[index];}
            ImageView      *GetDepthImageView       ()          {return attr->depth.view;}

    const   uint32_t        GetCurrentFrameIndices  ()          {return current_frame;}

            RenderPass *    GetRenderPass           ()          {return main_rp;}
            Framebuffer *   GetFramebuffer          (int index) {return main_fb[index];}

public:

    Buffer *            CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize size,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);
    
    VertexBuffer *      CreateVBO(VkFormat format,uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);

    IndexBuffer *       CreateIBO(VkIndexType index_type,uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);
    IndexBuffer *       CreateIBO16(uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(VK_INDEX_TYPE_UINT16,count,sharing_mode);}
    IndexBuffer *       CreateIBO32(uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(VK_INDEX_TYPE_UINT32,count,sharing_mode);}

#define CREATE_BUFFER_OBJECT(LargeName,type)  Buffer *Create##LargeName(VkDeviceSize size,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size,sharing_mode);}

    CREATE_BUFFER_OBJECT(UBO,UNIFORM)
    CREATE_BUFFER_OBJECT(SSBO,STORAGE)
    CREATE_BUFFER_OBJECT(INBO,INDIRECT)

#undef CREATE_BUFFER_OBJECT

    Material *      CreateMaterial(Shader *);

    CommandBuffer * CreateCommandBuffer();
    RenderPass *    CreateRenderPass(VkFormat color_format,VkFormat depth_format);
    Fence *         CreateFence();
    Semaphore *     CreateSem();

    bool AcquireNextImage   ();
    bool QueueSubmit        (CommandBuffer *);
    bool Wait               (bool wait_all=VK_TRUE,uint64_t time_out=HGL_NANO_SEC_PER_SEC*0.1);
    bool QueuePresent       ();
};//class Device
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDER_SURFACE_INCLUDE
