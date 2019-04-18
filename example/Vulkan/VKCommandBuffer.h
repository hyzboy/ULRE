#ifndef HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class RenderPass;
class Framebuffer;
class Pipeline;
class VertexInput;

class CommandBuffer
{
    VkDevice device;
    VkCommandPool pool;
    VkCommandBuffer cmd_buf;

    VkClearValue clear_values[2];
    VkRect2D render_area;

public:

    CommandBuffer(VkDevice dev,VkCommandPool cp,VkCommandBuffer cb);
    ~CommandBuffer();

    void SetRenderArea(const VkRect2D &ra){render_area=ra;}

    bool Bind(RenderPass *rp,Framebuffer *fb,Pipeline *p);
    bool Bind(VertexInput *vi);
};//class CommandBuffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
