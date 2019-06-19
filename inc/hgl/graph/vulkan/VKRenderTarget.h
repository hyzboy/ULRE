#ifndef HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE
#define HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKRenderPass.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>
VK_NAMESPACE_BEGIN
class RenderTarget
{
    Device *device;

    RenderPass *rp;
    Framebuffer *fb;

    VkExtent2D extent;

    List<ImageView *> colors;
    ImageView *depth;

private:

    friend class Device;

    RenderTarget(Device *dev,RenderPass *_rp,Framebuffer *_fb)
    {
        device=dev;
        rp=_rp;
        fb=_fb;
    }

public:

    ~RenderTarget()
    {
        if(fb)delete fb;
    }

    operator RenderPass *   (){return rp;}
    operator Framebuffer *  (){return fb;}
    operator VkRenderPass   (){return rp?rp->operator VkRenderPass():nullptr;}
    operator VkFramebuffer  (){return fb?fb->operator VkFramebuffer():nullptr;}

    const VkExtent2D &  GetExtent       ()const{return extent;}                                     ///<取得画面尺寸

    const uint          GetColorCount   ()const{colors.GetCount();}                                 ///<取得颜色成份数量
    const bool          IsExistDepth    ()const{return depth;}                                      ///<是否存在深度成份
};//class RenderTarget
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDER_TARGET_INCLUDE
