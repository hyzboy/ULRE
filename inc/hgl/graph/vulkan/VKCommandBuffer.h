#ifndef HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE

#include<hgl/graph/vulkan/VK.h>
VK_NAMESPACE_BEGIN
class CommandBuffer
{
    VkDevice device;
    VkCommandPool pool;
    VkCommandBuffer cmd_buf;

    VkClearValue clear_values[2];
    VkRect2D render_area;
    VkViewport viewport;

public:

    CommandBuffer(VkDevice dev,const VkExtent2D &extent,VkCommandPool cp,VkCommandBuffer cb);
    ~CommandBuffer();

    operator VkCommandBuffer(){return cmd_buf;}
    operator const VkCommandBuffer()const{return cmd_buf;}
    operator const VkCommandBuffer *()const{return &cmd_buf;}

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

    //以上设定在Begin开始后即不可改变

    bool Begin();

    void PipelineBarrier(
        VkPipelineStageFlags                        srcStageMask,
        VkPipelineStageFlags                        dstStageMask,
        VkDependencyFlags                           dependencyFlags,
        uint32_t                                    memoryBarrierCount,
        const VkMemoryBarrier*                      pMemoryBarriers,
        uint32_t                                    bufferMemoryBarrierCount,
        const VkBufferMemoryBarrier*                pBufferMemoryBarriers,
        uint32_t                                    imageMemoryBarrierCount,
        const VkImageMemoryBarrier*                 pImageMemoryBarriers)
    {
        vkCmdPipelineBarrier(cmd_buf,srcStageMask,dstStageMask,dependencyFlags,memoryBarrierCount,pMemoryBarriers,bufferMemoryBarrierCount,pBufferMemoryBarriers,imageMemoryBarrierCount,pImageMemoryBarriers);
    }

    void CopyBufferToImage(
        VkBuffer                                    srcBuffer,
        VkImage                                     dstImage,
        VkImageLayout                               dstImageLayout,
        uint32_t                                    regionCount,
        const VkBufferImageCopy*                    pRegions)
    {
        vkCmdCopyBufferToImage(cmd_buf,srcBuffer,dstImage,dstImageLayout,regionCount,pRegions);
    }

    bool BeginRenderPass(RenderPass *rp,Framebuffer *fb);
    bool Bind(Pipeline *p);
    bool Bind(DescriptorSets *);
    bool Bind(Renderable *);
    void EndRenderPass();
    bool End();

public:

    void SetDepthBias(float constant_factor,float clamp,float slope_factor);
    void SetDepthBounds(float min_db,float max_db);
    void SetStencilCompareMask(VkStencilFaceFlags faceMask,uint32_t compareMask);
    void SetStencilWriteMask(VkStencilFaceFlags faceMask,uint32_t compareMask);
    void SetStencilReference(VkStencilFaceFlags faceMask,uint32_t compareMask);

    void SetBlendConstants(const float constants[4]);

    void SetLineWidth(float);

public:

    void Draw(const uint32_t vertex_count);
    void Draw(const uint32_t vertex_count,const uint32_t instance_count,const uint32_t first_vertex=0,const uint32_t first_instance=0);
    void DrawIndexed(const uint32_t index_count);
    void DrawIndexed(const uint32_t index_count,const uint32_t instance_count,const uint32_t first_index=0,const uint32_t vertex_offset=0,const uint32_t first_instance=0);
};//class CommandBuffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
