#ifndef HGL_GRAPH_RENDER_SURFACE_INCLUDE
#define HGL_GRAPH_RENDER_SURFACE_INCLUDE

#include<hgl/type/List.h>
#include<hgl/type/BaseString.h>
#include<hgl/type/Map.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKDeviceAttribute.h>
#include<hgl/graph/vulkan/VKSwapchain.h>
#include<hgl/graph/VertexBufferCreater.h>

VK_NAMESPACE_BEGIN
class Device
{
    DeviceAttribute *attr;

    Fence *texture_fence;

    VkSubmitInfo texture_submit_info;
    CommandBuffer *texture_cmd_buf;

    Swapchain *swapchain;

private:

    friend Device *CreateRenderDevice(VkInstance inst,const PhysicalDevice *physical_device,VkSurfaceKHR surface,const VkExtent2D &extent);

    Device(DeviceAttribute *da);

public:

    virtual ~Device();

    operator    VkDevice                                ()      {return attr->device;}
                DeviceAttribute *   GetDeviceAttribute  ()      {return attr;}

                VkSurfaceKHR        GetSurface          ()      {return attr->surface;}
                VkDevice            GetDevice           ()      {return attr->device;}
    const       PhysicalDevice *    GetPhysicalDevice   ()const {return attr->physical_device;}

                VkDescriptorPool    GetDescriptorPool   ()      {return attr->desc_pool;}
                VkPipelineCache     GetPipelineCache    ()      {return attr->pipeline_cache;}

    const       VkFormat            GetSurfaceFormat    ()const {return attr->format;}
                VkQueue             GetGraphicsQueue    ()      {return attr->graphics_queue;}

                Swapchain *         GetSwapchain        ()      {return swapchain;}

public:

                bool                Resize              (const VkExtent2D &);
                bool                Resize              (const uint32_t &w,const uint32_t &h)
                {
                    VkExtent2D extent={w,h};

                    return Resize(extent);
                }

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

    Texture2D *CreateAttachmentTexture(const VkFormat video_format,uint32_t width,uint32_t height,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout)
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
                                        VK_IMAGE_ASPECT_DEPTH_BIT,//|VK_IMAGE_ASPECT_STENCIL_BIT,
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    Texture2D *CreateTexture2D( const VkFormat video_format,Buffer *buf,uint32_t width,uint32_t height,
                                const VkImageAspectFlags    aspectMask  =VK_IMAGE_ASPECT_COLOR_BIT,
                                const uint                  usage       =VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                                const VkImageLayout         image_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    Texture2D *CreateTexture2D( const VkFormat video_format,void *data,uint32_t width,uint32_t height,uint32_t size,
                                const VkImageAspectFlags    aspectMask  =VK_IMAGE_ASPECT_COLOR_BIT,
                                const uint                  usage       =VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                                const VkImageLayout         image_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    bool ChangeTexture2D(Texture2D *,Buffer *buf,uint32_t left,uint32_t top,uint32_t width,uint32_t height);
    bool ChangeTexture2D(Texture2D *,void *data,uint32_t left,uint32_t top,uint32_t width,uint32_t height,uint32_t size);
    Sampler *CreateSampler(VkSamplerCreateInfo *);

    ShaderModuleManage *CreateShaderModuleManage();

public: //Command Buffer 相关

    CommandBuffer * CreateCommandBuffer(const VkExtent2D &extent,const uint32_t atta_count);
    
    bool CreateAttachment(      List<VkAttachmentReference> &ref_list,
                                List<VkAttachmentDescription> &desc_list,
                                const List<VkFormat> &color_format,
                                const VkFormat depth_format,
                                const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)const;

    bool CreateColorAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const List<VkFormat> &color_format,const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)const;
    bool CreateDepthAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const VkFormat &depth_format,const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)const;

    void CreateSubpassDependency(List<VkSubpassDependency> &dependency,const uint32_t count)const;
    void CreateSubpassDescription(VkSubpassDescription &,const List<VkAttachmentReference> &)const;

    RenderPass *    CreateRenderPass(   const List<VkAttachmentDescription> &desc_list,
                                        const List<VkSubpassDescription> &subpass,
                                        const List<VkSubpassDependency> &dependency,
                                        const List<VkFormat> &color_format,
                                        const VkFormat depth_format,
                                        const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                        const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)const;

    RenderPass *    CreateRenderPass(   const VkFormat color_format,
                                        const VkFormat depth_format,
                                        const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                        const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)const;

    Fence *         CreateFence(bool);
    Semaphore *     CreateSem();

public:

    bool SubmitTexture      (const VkCommandBuffer *cmd_bufs,const uint32_t count=1);           ///<提交纹理处理到队列
};//class Device
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDER_SURFACE_INCLUDE
