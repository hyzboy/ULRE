#ifndef HGL_GRAPH_VULKAN_FRAMEBUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_FRAMEBUFFER_INCLUDE

#include<hgl/graph/VK.h>
VK_NAMESPACE_BEGIN
class Framebuffer
{
    VkDevice device;
    VkFramebuffer frame_buffer;
    VkRenderPass render_pass;

    VkExtent2D extent;
    uint32_t attachment_count;
    uint32_t color_count;
    bool has_depth;

private:

    friend class TextureManager;

    Framebuffer(VkDevice,VkFramebuffer,const VkExtent2D &,VkRenderPass,uint32_t color_count,bool depth);

public:

    ~Framebuffer();

    operator VkFramebuffer(){return frame_buffer;}

    const   VkFramebuffer   GetFramebuffer      ()const{return frame_buffer;}
    const   VkRenderPass    GetRenderPass       ()const{return render_pass;}

    const   VkExtent2D &    GetExtent           ()const{return extent;}

    const   uint32_t        GetAttachmentCount  ()const{return attachment_count;}                   ///<获取渲染目标成分数量
    const   uint32_t        GetColorCount       ()const{return color_count;}                        ///<取得颜色成分数量
    const   bool            HasDepth            ()const{return has_depth;}                          ///<是否包含深度成分
};//class Framebuffer

using FBO=Framebuffer;
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_FRAMEBUFFER_INCLUDE
