#ifndef HGL_GRAPH_VULKAN_FRAMEBUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_FRAMEBUFFER_INCLUDE

#include<hgl/graph/vulkan/VK.h>
VK_NAMESPACE_BEGIN
class Framebuffer
{
    VkDevice device;
    VkFramebuffer frame_buffer;
    VkFramebufferCreateInfo *fb_info;

    VkExtent2D extent;
    uint32_t color_count;
    bool has_depth;

private:

    friend Framebuffer *CreateFramebuffer(Device *dev,RenderPass *rp,ImageView **color_list,const uint color_count,ImageView *depth);

    Framebuffer(VkDevice dev,VkFramebuffer fb,VkFramebufferCreateInfo *fb_create_info,bool depth)
    {
        device=dev;
        frame_buffer=fb;
        fb_info=fb_create_info;

        extent.width=fb_info->width;
        extent.height=fb_info->height;

        has_depth=depth;
        if(has_depth)
            color_count=fb_info->attachmentCount-1;
        else
            color_count=fb_info->attachmentCount;
    }

public:

    ~Framebuffer();

            VkFramebuffer   GetFramebuffer      (){return frame_buffer;}
            VkRenderPass    GetRenderPass       (){return fb_info->renderPass;}

    const   VkExtent2D &    GetExtent           ()const{return extent;}

    const   uint32_t        GetAttachmentCount  ()const{return fb_info->attachmentCount;}           ///<获取渲染目标成分数量
    const   uint32_t        GetColorCount       ()const{return color_count;}                        ///<取得颜色成分数量
    const   bool            HasDepth            ()const{return has_depth;}                          ///<是否包含深度成分
};//class Framebuffer

Framebuffer *CreateFramebuffer(Device *,RenderPass *,List<ImageView *> &color,ImageView *depth);
Framebuffer *CreateFramebuffer(Device *,RenderPass *,List<ImageView *> &image_view_list);
Framebuffer *CreateFramebuffer(Device *,RenderPass *,ImageView *color,ImageView *depth=nullptr);
Framebuffer *CreateFramebuffer(Device *,RenderPass *,ImageView *depth);
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_FRAMEBUFFER_INCLUDE
