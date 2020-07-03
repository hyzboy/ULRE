#ifndef HGL_GRAPH_RENDER_SURFACE_INCLUDE
#define HGL_GRAPH_RENDER_SURFACE_INCLUDE

#include<hgl/type/List.h>
#include<hgl/type/BaseString.h>
#include<hgl/type/Map.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKDeviceAttribute.h>
#include<hgl/graph/vulkan/VKSwapchain.h>
#include<hgl/graph/vulkan/VKRenderTarget.h>
#include<hgl/graph/VertexBufferCreater.h>

namespace hgl
{
    namespace graph
    {
        class TileData;
    }//namespace graph
}//namespace hgl

VK_NAMESPACE_BEGIN
class Device
{
    DeviceAttribute *attr;

    SubmitQueue *textureSQ;
    CommandBuffer *texture_cmd_buf;

    Swapchain *swapchain;
    SwapchainRenderTarget *swapchainRT;

    bool CreateSwapchainColorTexture();
    bool CreateSwapchainDepthTexture();

    bool CreateSwapchain(const VkExtent2D &acquire_extent);

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

    SwapchainRenderTarget *         GetSwapchainRT      ()      {return swapchainRT;}

public:

                const VkExtent2D &  GetSwapchainSize    ()const {return swapchain->extent;}

                bool                Resize              (const VkExtent2D &);
                bool                Resize              (const uint32_t &w,const uint32_t &h)
                {
                    VkExtent2D extent={w,h};

                    return Resize(extent);
                }

public: //内存相关

    Memory *CreateMemory(const VkMemoryRequirements &,const uint32_t properties);

private: //Buffer相关

    bool                CreateBuffer(BufferData *buf,VkBufferUsageFlags buf_usage,VkDeviceSize size,const void *data,VkSharingMode sharing_mode);

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

public: //Image

    VkImage CreateImage1D           (const VkFormat format,const uint32_t width,const uint usage,const VkImageTiling tiling);
    VkImage CreateImage1DArray      (const VkFormat format,const uint32_t width,const uint32_t layer,const uint usage,const VkImageTiling tiling);
    VkImage CreateImage2D           (const VkFormat format,const uint32_t width,const uint32_t height,const uint usage,const VkImageTiling tiling);
    VkImage CreateImage2DArray      (const VkFormat format,const uint32_t width,const uint32_t height,const uint32_t layer,const uint usage,const VkImageTiling tiling);
    VkImage CreateImage3D           (const VkFormat format,const uint32_t width,const uint32_t height,const uint32_t depth,const uint usage,const VkImageTiling tiling);
    VkImage CreateImageCubemap      (const VkFormat format,const uint32_t width,const uint32_t height,const uint usage,const VkImageTiling tiling);

    void DestoryImage(VkImage);

    Memory *CreateMemory(VkImage,const uint32 flag=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

public: //Texture

    Texture2D *CreateTexture2D(Memory *mem,VkImage image,ImageView *image_view,VkImageLayout image_layout,VkImageTiling tiling);
    Texture2D *CreateTexture2D(VkFormat format,uint32_t width,uint32_t height,VkImageAspectFlagBits aspectMask,VkImage image,VkImageLayout image_layout,VkImageTiling tiling=VK_IMAGE_TILING_OPTIMAL);

    Texture2D *CreateTexture2D(const VkFormat format,uint32_t width,uint32_t height,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout,VkImageTiling tiling=VK_IMAGE_TILING_OPTIMAL);

    Texture2D *CreateTexture2DColor(const VkFormat video_format,uint32_t width,uint32_t height,const VkImageTiling tiling=VK_IMAGE_TILING_OPTIMAL)
    {
        return CreateTexture2D(video_format,width,height,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                (tiling==VK_IMAGE_TILING_OPTIMAL?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_GENERAL),
                tiling);
    }

    Texture2D *CreateTexture2DDepth(const VkFormat video_format,uint32_t width,uint32_t height,const VkImageTiling tiling=VK_IMAGE_TILING_OPTIMAL)
    {
        return CreateTexture2D(video_format,width,height,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                (tiling==VK_IMAGE_TILING_OPTIMAL?VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:VK_IMAGE_LAYOUT_GENERAL),
                tiling);
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
                                const VkImageLayout         image_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                const VkImageTiling         tiling      =VK_IMAGE_TILING_OPTIMAL);

    Texture2D *CreateTexture2D( const VkFormat video_format,void *data,uint32_t width,uint32_t height,uint32_t size,
                                const VkImageAspectFlags    aspectMask  =VK_IMAGE_ASPECT_COLOR_BIT,
                                const uint                  usage       =VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
                                const VkImageLayout         image_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                const VkImageTiling         tiling      =VK_IMAGE_TILING_OPTIMAL);

    bool ChangeTexture2D(Texture2D *,Buffer *buf,uint32_t left,uint32_t top,uint32_t width,uint32_t height);
    bool ChangeTexture2D(Texture2D *,void *data,uint32_t left,uint32_t top,uint32_t width,uint32_t height,uint32_t size);

public: //

    Sampler *CreateSampler(VkSamplerCreateInfo *);

    ShaderModuleManage *CreateShaderModuleManage();

public: //Command Buffer 相关

    CommandBuffer * CreateCommandBuffer(const VkExtent2D &extent,const uint32_t atta_count);
    
    void CreateAttachmentReference(VkAttachmentReference *ref_list,uint start,uint count,VkImageLayout layout)const;

    void CreateColorAttachmentReference(List<VkAttachmentReference> &ref_list,  uint start,uint count )const{ref_list.SetCount(count);  CreateAttachmentReference(ref_list.GetData(),  start,count,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);}
    void CreateDepthAttachmentReference(     VkAttachmentReference  *depth_ref, uint index            )const{                           CreateAttachmentReference(depth_ref,           index,1    ,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);}
    void CreateInputAttachment(         List<VkAttachmentReference> &ref_list,  uint start,uint count )const{ref_list.SetCount(count);  CreateAttachmentReference(ref_list.GetData(),  start,count,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);}

    bool CreateAttachment(      List<VkAttachmentDescription> &color_output_desc_list,
                                const List<VkFormat> &color_format,
                                const VkFormat depth_format,
                                const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)const;

    bool CreateColorAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const List<VkFormat> &color_format,const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)const;
    bool CreateDepthAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const VkFormat &depth_format,const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)const;

    void CreateSubpassDependency(VkSubpassDependency *);
    void CreateSubpassDependency(List<VkSubpassDependency> &dependency,const uint32_t count)const;
    void CreateSubpassDescription(VkSubpassDescription &,const List<VkAttachmentReference> &color_ref_list,VkAttachmentReference *depth_ref=nullptr)const;

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

    Fence *             CreateFence(bool);
    vulkan::Semaphore * CreateSem();

public:

    bool SubmitTexture      (const VkCommandBuffer *cmd_bufs,const uint32_t count=1);           ///<提交纹理处理到队列

    RenderTarget *CreateRenderTarget(Framebuffer *);

    TileData *CreateTileData(const VkFormat video_format,const uint width,const uint height,const uint count);               ///<创建一个Tile数据集
};//class Device

Device *CreateRenderDevice(Instance *inst,Window *win,const PhysicalDevice *physical_device=nullptr);
VK_NAMESPACE_END
#endif//HGL_GRAPH_RENDER_SURFACE_INCLUDE
