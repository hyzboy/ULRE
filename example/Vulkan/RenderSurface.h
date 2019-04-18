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

    friend RenderSurface *CreateRenderSuface(VkInstance,const PhysicalDevice *,Window *);

    RenderSurface(RefRenderSurfaceAttribute &ref_rsa)
    {
        rsa=ref_rsa;
    }

public:

    virtual ~RenderSurface()=default;

            VkSurfaceKHR    GetSurface          ()      {return rsa->surface;}
            VkDevice        GetDevice           ()      {return rsa->device;}
    const   PhysicalDevice *GetPhysicalDevice   ()const {return rsa->physical_device;}

public:

    Buffer *            CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize size,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);
    VertexBuffer *      CreateBuffer(VkBufferUsageFlags buf_usage,VkFormat format,uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);

#define CREATE_FORMAT_BUFFER_OBJECT(LargeName,type)  Buffer *Create##LargeName(VkFormat format,uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,format,count,sharing_mode);}
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
};//class RenderSurface
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDER_SURFACE_INCLUDE
