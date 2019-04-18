#ifndef HGL_GRAPH_VULKAN_FRAMEBUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_FRAMEBUFFER_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class Device;
class RenderPass;

class Framebuffer
{
    VkDevice device;
    VkFramebuffer frame_buffer;

private:

    friend Framebuffer *CreateFramebuffer(Device *,RenderPass *,VkImageView color,VkImageView depth);;

    Framebuffer(VkDevice dev,VkFramebuffer fb)
    {
        device=dev;
        frame_buffer=fb;
    }

public:

    ~Framebuffer();

    operator VkFramebuffer(){return frame_buffer;}
};//class Framebuffer

Framebuffer *CreateFramebuffer(Device *,RenderPass *,VkImageView color,VkImageView depth);
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_FRAMEBUFFER_INCLUDE
