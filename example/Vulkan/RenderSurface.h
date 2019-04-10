#ifndef HGL_GRAPH_RENDER_SURFACE_INCLUDE
#define HGL_GRAPH_RENDER_SURFACE_INCLUDE

#include<hgl/type/List.h>
#include"VK.h"
#include"Window.h"
#include"RenderSurfaceAttribute.h"
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

    CommandBuffer *     CreateCommandBuffer ();
};//class RenderSurface
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDER_SURFACE_INCLUDE
