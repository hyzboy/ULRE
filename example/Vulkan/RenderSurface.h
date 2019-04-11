#ifndef HGL_GRAPH_RENDER_SURFACE_INCLUDE
#define HGL_GRAPH_RENDER_SURFACE_INCLUDE

#include<hgl/type/List.h>
#include"VK.h"
#include"Window.h"
#include"RenderSurfaceAttribute.h"
#include"VKBuffer.h"
#include"VKCommandBuffer.h"

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

    Buffer *            CreateUBO(VkDeviceSize size,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE)
    {
        return CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,size,sharing_mode);
    }

    CommandBuffer *     CreateCommandBuffer ();
};//class RenderSurface
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDER_SURFACE_INCLUDE
