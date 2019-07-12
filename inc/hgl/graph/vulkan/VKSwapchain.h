#ifndef HGL_GRAPH_VULKAN_SWAP_CHAIN_INCLUDE
#define HGL_GRAPH_VULKAN_SWAP_CHAIN_INCLUDE

#include<hgl/graph/Vulkan/VK.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKFence.h>
#include<hgl/graph/vulkan/VKSemaphore.h>
VK_NAMESPACE_BEGIN
class Swapchain
{
protected:

    Device *device;
    
    VkExtent2D              extent;

    VkQueue                 graphics_queue  =VK_NULL_HANDLE;
    VkSwapchainKHR          swap_chain      =VK_NULL_HANDLE;

    uint32_t                swap_chain_count;

    ObjectList<Texture2D>   sc_texture;
    Texture2D *             sc_depth    =nullptr;

protected:

    VkPipelineStageFlags pipe_stage_flags;

    uint32_t current_frame;
    ObjectList<Framebuffer> render_frame;

    uint32_t current_fence;
    ObjectList<Fence> fence_list;

    RenderPass *main_rp =nullptr;
    
    Semaphore *present_complete_semaphore   =nullptr,
              *render_complete_semaphore    =nullptr;

    VkSubmitInfo submit_info;
    VkPresentInfoKHR present_info;

public:

    const   VkExtent2D &    GetExtent               ()const     {return extent;}
    const   uint32_t        GetImageCount           ()const     {return sc_texture.GetCount();}

            RenderPass *    GetMainRenderPass       ()          {return main_rp;}
            Framebuffer *   GetFramebuffer          (int index) {return render_frame[index];}
    const   uint32_t        GetCurrentFrameIndices  ()          {return current_frame;}
            Framebuffer *   GetCurrentFramebuffer   ()          {return render_frame[current_frame];}

public:

    Swapchain(Device *);
    virtual ~Swapchain();

    void Clear              ();

    void Recreate           ();

    bool Wait               (bool wait_all=VK_TRUE,uint64_t time_out=HGL_NANO_SEC_PER_SEC*0.1); ///<等待队列完成

    /**
     * 请求获得下一帧的索引，并将确认信息发送到指定信号
     */
    bool AcquireNextImage   (VkSemaphore);                                                      ///<请求获得下一帧的索引

    /**
     * 提交一批绘制指令
     * @param cmd_list 绘制指令
     * @param wait_sems 指令开始前要等待的确认的信号
     * @param complete_semaphores 绘制完成后发送的信号
     */
    bool SubmitDraw         (List<VkCommandBuffer> &cmd_list,List<VkSemaphore> &wait_sems,List<VkSemaphore> &complete_semaphores);       ///<提交绘制指令

    bool SubmitTexture      (const VkCommandBuffer *cmd_bufs,const uint32_t count=1);           ///<提交纹理处理到队列

    bool PresentBackbuffer  ();                                                                 ///<等待绘制队列完成，并将后台缓冲区呈现到前台
};//class Swapchain
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_SWAP_CHAIN_INCLUDE
