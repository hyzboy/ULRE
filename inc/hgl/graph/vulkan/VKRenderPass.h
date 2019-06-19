#ifndef HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE
#define HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE

#include<hgl/graph/vulkan/VK.h>
VK_NAMESPACE_BEGIN
/**
 * RenderPass功能封装<br>
 * RenderPass在创建时，需要指定输入的color imageview与depth imageview象素格式，
 * 在随后创建Framebuffer时，需要同时指定RenderPass与ColorImageView,DepthImageView，象素格式要一致。
 */
class RenderPass
{
    VkDevice device;
    VkRenderPass render_pass;

    List<VkFormat> color_formats;
    VkFormat depth_format;

private:

    friend class Device;

    RenderPass(VkDevice d,VkRenderPass rp,const List<VkFormat> &cf,VkFormat df)
    {
        device=d;
        render_pass=rp;
        color_formats=cf;
        depth_format=df;
    }

    RenderPass(VkDevice d,VkRenderPass rp,VkFormat cf,VkFormat df)
    {
        device=d;
        render_pass=rp;
        color_formats.Add(cf);
        depth_format=df;
    }

public:

    virtual ~RenderPass();

    operator VkRenderPass(){return render_pass;}

    const List<VkFormat> &  GetColorFormat()const{return color_formats;}
    const VkFormat          GetDepthFormat()const{return depth_format;}
};//class RenderPass
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_RENDER_PASS_INCLUDE
