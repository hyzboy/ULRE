#ifndef HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE
#define HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class Framebuffer;
class ImageView;

/**
 * RenderPass功能封装<br>
 * RenderPass在创建时，需要指定输入的color imageview与depth imageview象素格式，
 * 在随后创建Framebuffer时，需要同时指定RenderPass与ColorImageView,DepthImageView，象素格式要一致。
 */
class RenderPass
{
    VkDevice device;
    VkRenderPass render_pass;

    VkFormat color_format,depth_format;

public:

    RenderPass(VkDevice d,VkRenderPass rp,VkFormat cf,VkFormat df)
    {
        device=d;
        render_pass=rp;
        color_format=cf;
        depth_format=df;
    }
    virtual ~RenderPass();

    operator VkRenderPass(){return render_pass;}

    const VkFormat GetColorFormat()const{return color_format;}
    const VkFormat GetDepthFormat()const{return depth_format;}
};//class RenderPass
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE
