#ifndef HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE

#include"VK.h"
VK_NAMESPACE_BEGIN
class RenderPass;
class Framebuffer;
class Pipeline;
class PipelineLayout;
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
    void SetClearColor(float r,float g,float b,float a=1.0f)
    {
        clear_values[0].color.float32[0]=r;
        clear_values[0].color.float32[1]=g;
        clear_values[0].color.float32[2]=b;
        clear_values[0].color.float32[3]=a;
    }

    void SetClearDepthStencil(float d=1.0f,float s=0)
    {
        clear_values[1].depthStencil.depth=d;
        clear_values[1].depthStencil.stencil=s;
    }

    bool Begin(RenderPass *rp,Framebuffer *fb);
    bool Bind(Pipeline *p);
    bool Bind(PipelineLayout *pl);
    bool Bind(VertexInput *vi,const VkDeviceSize offset=0);
    void Draw(const uint32_t vertex_count);
    void Draw(const uint32_t vertex_count,const uint32_t instance_count,const uint32_t first_vertex=0,const uint32_t first_instance=0);
    void End();
};//class CommandBuffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
