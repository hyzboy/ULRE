#ifndef HGL_GRAPH_RENDER_SURFACE_INCLUDE
#define HGL_GRAPH_RENDER_SURFACE_INCLUDE

#include<hgl/type/List.h>
#include"VK.h"
#include"Window.h"
#include"RenderSurfaceAttribute.h"
#include"VKBuffer.h"
#include"VKCommandBuffer.h"
//#include"VKDescriptorSet.h"
#include"VKRenderPass.h"

VK_NAMESPACE_BEGIN

using RefRenderSurfaceAttribute=SharedPtr<RenderSurfaceAttribute>;

class RenderSurface
{
    RefRenderSurfaceAttribute rsa;

private:

    friend RenderSurface *CreateRenderSuface(VkInstance,VkPhysicalDevice,Window *);

    RenderSurface(RefRenderSurfaceAttribute &ref_rsa)
    {
        rsa=ref_rsa;
    }

public:

    virtual ~RenderSurface()=default;

    VkPhysicalDevice    GetPhysicalDevice   () { return rsa->physical_device; }
    VkSurfaceKHR        GetSurface          () { return rsa->surface; }

public:

    Buffer *            CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize size,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);

#define CREATE_BUFFER_OBJECT(LargeName,type)  Buffer *Create##LargeName(VkDeviceSize size,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size,sharing_mode);}

    CREATE_BUFFER_OBJECT(UBO,UNIFORM)
    CREATE_BUFFER_OBJECT(VBO,VERTEX)
    CREATE_BUFFER_OBJECT(IBO,INDEX)
    CREATE_BUFFER_OBJECT(SSBO,STORAGE)
    CREATE_BUFFER_OBJECT(INBO,INDIRECT)

#undef CREATE_BUFFER_OBJECT

    CommandBuffer *     CreateCommandBuffer ();

//    DescriptorSet *     CreateDescSet(int);

    RenderPass *CreateRenderPass();
};//class RenderSurface
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDER_SURFACE_INCLUDE
