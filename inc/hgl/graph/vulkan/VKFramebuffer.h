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

    ObjectList<Texture2D> color_texture;
    Texture2D *depth_texture;

private:

    friend Framebuffer *CreateFramebuffer(VkDevice dev,RenderPass *rp,ImageView **color_list,const uint color_count,ImageView *depth);

    Framebuffer(VkDevice dev,VkFramebuffer fb,VkFramebufferCreateInfo *fb_create_info,bool depth);

public:

    ~Framebuffer();

    const   VkFramebuffer   GetFramebuffer      ()const{return frame_buffer;}
    const   VkRenderPass    GetRenderPass       ()const{return fb_info->renderPass;}

    const   VkExtent2D &    GetExtent           ()const{return extent;}

    const   uint32_t        GetAttachmentCount  ()const{return fb_info->attachmentCount;}           ///<获取渲染目标成分数量
    const   uint32_t        GetColorCount       ()const{return color_count;}                        ///<取得颜色成分数量
    const   bool            HasDepth            ()const{return has_depth;}                          ///<是否包含深度成分

            Texture2D *     GetColorTexture     (const int index=0){return color_texture[index];}
            Texture2D *     GetDepthTexture     (){return depth_texture;}
};//class Framebuffer

Framebuffer *CreateFramebuffer(VkDevice,RenderPass *,List<ImageView *> &color,ImageView *depth);
Framebuffer *CreateFramebuffer(VkDevice,RenderPass *,ImageView *color,ImageView *depth);
Framebuffer *CreateFramebuffer(VkDevice,RenderPass *,ImageView *);
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_FRAMEBUFFER_INCLUDE
