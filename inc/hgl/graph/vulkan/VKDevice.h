#ifndef HGL_GRAPH_RENDER_SURFACE_INCLUDE
#define HGL_GRAPH_RENDER_SURFACE_INCLUDE

#include<hgl/type/List.h>
#include<hgl/type/BaseString.h>
#include<hgl/type/Map.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKDeviceAttribute.h>
#include<hgl/graph/vulkan/VKFramebuffer.h>
#include<hgl/graph/vulkan/VKFence.h>
#include<hgl/graph/vulkan/VKSemaphore.h>
#include<hgl/graph/VertexBufferCreater.h>

VK_NAMESPACE_BEGIN
class Device
{
    DeviceAttribute *attr;

    uint swap_chain_count;

    Semaphore *present_complete_semaphore=nullptr,
              *render_complete_semaphore=nullptr;

    VkPipelineStageFlags pipe_stage_flags;
    VkSubmitInfo submit_info;

    Fence *texture_fence;

    VkSubmitInfo texture_submit_info;
    CommandBuffer *texture_cmd_buf;

    RenderPass *main_rp;

    uint32_t current_frame;
    ObjectList<Framebuffer> render_frame;

    uint32_t current_fence;
    ObjectList<Fence> fence_list;

    VkPresentInfoKHR present_info;

private:

    void RecreateDevice();

private:

    friend Device *CreateRenderDevice(VkInstance inst,const PhysicalDevice *physical_device,VkSurfaceKHR surface,uint width,uint height);

    Device(DeviceAttribute *da);

public:

    virtual ~Device();

    operator VkDevice                           ()      {return attr->device;}

            VkSurfaceKHR    GetSurface          ()      {return attr->surface;}
            VkDevice        GetDevice           ()      {return attr->device;}
    const   PhysicalDevice *GetPhysicalDevice   ()const {return attr->physical_device;}
    const   VkExtent2D &    GetExtent           ()const {return attr->swapchain_extent;}

    VkDescriptorPool        GetDescriptorPool   ()      {return attr->desc_pool;}
    VkPipelineCache         GetPipelineCache    ()      {return attr->pipeline_cache;}

public:

    const   uint32_t        GetSwapChainImageCount  ()const     {return attr->sc_image_views.GetCount();}
            ImageView      *GetColorImageView       (int index) {return attr->sc_image_views[index];}
            ImageView      *GetDepthImageView       ()          {return attr->depth.view;}

            RenderPass *    GetRenderPass           ()          {return main_rp;}
            Framebuffer *   GetFramebuffer          (int index) {return render_frame[index];}
    const   uint32_t        GetCurrentFrameIndices  ()          {return current_frame;}
            Framebuffer *   GetCurrentFramebuffer   ()          {return render_frame[current_frame];}

    bool                    Resize                  (uint,uint);

public: //内存相关

    Memory *Device::CreateMemory(const VkMemoryRequirements &,uint32_t properties);

public: //Buffer相关

    Buffer *            CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize size,const void *data,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);
    Buffer *            CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize size,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateBuffer(buf_usage,size,nullptr,sharing_mode);}

    VertexBuffer *      CreateVBO(VkFormat format,uint32_t count,const void *data,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);
    VertexBuffer *      CreateVBO(VkFormat format,uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateVBO(format,count,nullptr,sharing_mode);}
    VertexBuffer *      CreateVBO(const VertexBufferCreater *vbc,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateVBO(vbc->GetDataType(),vbc->GetCount(),vbc->GetData(),sharing_mode);}

    IndexBuffer *       CreateIBO(VkIndexType index_type,uint32_t count,const void *data,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);
    IndexBuffer *       CreateIBO16(uint32_t count,const uint16 *data,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(VK_INDEX_TYPE_UINT16,count,(void *)data,sharing_mode);}
    IndexBuffer *       CreateIBO32(uint32_t count,const uint32 *data,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(VK_INDEX_TYPE_UINT32,count,(void *)data,sharing_mode);}

    IndexBuffer *       CreateIBO(VkIndexType index_type,uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(index_type,count,nullptr,sharing_mode);}
    IndexBuffer *       CreateIBO16(uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(VK_INDEX_TYPE_UINT16,count,nullptr,sharing_mode);}
    IndexBuffer *       CreateIBO32(uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(VK_INDEX_TYPE_UINT32,count,nullptr,sharing_mode);}

#define CREATE_BUFFER_OBJECT(LargeName,type)    Buffer *Create##LargeName(VkDeviceSize size,void *data,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size,data,sharing_mode);}  \
                                                Buffer *Create##LargeName(VkDeviceSize size,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateBuffer(VK_BUFFER_USAGE_##type##_BUFFER_BIT,size,nullptr,sharing_mode);}

    CREATE_BUFFER_OBJECT(UBO,UNIFORM)
    CREATE_BUFFER_OBJECT(SSBO,STORAGE)
    CREATE_BUFFER_OBJECT(INBO,INDIRECT)

#undef CREATE_BUFFER_OBJECT

public: //material相关

    Texture2D *CreateRefTexture2D(uint32_t width,uint32_t height,VkFormat format,VkImageAspectFlagBits flag,VkImage image,VkImageLayout image_layout,VkImageView image_view);

    Texture2D *CreateTexture2D(const VkFormat video_format,uint32_t width,uint32_t height,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout);

    Texture2D *CreateTexture2DColor(const VkFormat video_format,uint32_t width,uint32_t height)
    {
        return CreateTexture2D(video_format,width,height,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    Texture2D *CreateTexture2DDepth(const VkFormat video_format,uint32_t width,uint32_t height)
    {
        return CreateTexture2D(video_format,width,height,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    Texture2D *CreateAttachmentTexture(const VkFormat video_format,uint32_t width,uint32_t height,const VkImageAspectFlags aspectMask,const VkImageUsageFlagBits usage,const VkImageLayout image_layout)
    {
        return CreateTexture2D(video_format,width,height,aspectMask,usage|VK_IMAGE_USAGE_SAMPLED_BIT,image_layout);
    }

    Texture2D *CreateAttachmentTextureColor(const VkFormat video_format,uint32_t width,uint32_t height)
    {
        return CreateAttachmentTexture( video_format,width,height,
                                        VK_IMAGE_ASPECT_COLOR_BIT,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    Texture2D *CreateAttachmentTextureDepth(const VkFormat video_format,uint32_t width,uint32_t height)
    {
        return CreateAttachmentTexture( video_format,width,height,
                                        VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    Texture2D *CreateTexture2D( const VkFormat video_format,void *data,uint32_t width,uint32_t height,uint32_t size,
                                const VkImageAspectFlags    aspectMask  =VK_IMAGE_ASPECT_COLOR_BIT,
                                const uint                  usage       =VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                                const VkImageLayout         image_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    bool ChangeTexture2D(Texture2D *,void *data,uint32_t left,uint32_t top,uint32_t width,uint32_t height,uint32_t size);
    Sampler *CreateSampler(VkSamplerCreateInfo *);

    ShaderModuleManage *CreateShaderModuleManage();

public: //Command Buffer 相关

    CommandBuffer * CreateCommandBuffer();

    RenderPass *    CreateRenderPass(   List<VkFormat> color_format,VkFormat depth_format,
                                        VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    RenderPass *    CreateRenderPass(   VkFormat color_format,VkFormat depth_format,
                                        VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                        VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    Fence *         CreateFence(bool);
    Semaphore *     CreateSem();

public: //提交相关

    bool Wait               (bool wait_all=VK_TRUE,uint64_t time_out=HGL_NANO_SEC_PER_SEC*0.1); ///<等待队列完成
    bool AcquireNextImage   ();                                                                 ///<请求切换到下一帧(注意此函数调用并不代表画面会出现)
    bool SubmitDraw         (const VkCommandBuffer *,const uint32_t count=1);                   ///<提交绘制指令
    bool SubmitTexture      (const VkCommandBuffer *cmd_bufs,const uint32_t count=1);           ///<提交纹理处理到队列
    bool QueuePresent       ();                                                                 ///<等待队列完成，并将画面呈现出来
};//class Device
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDER_SURFACE_INCLUDE
